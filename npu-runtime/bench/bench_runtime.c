/*
 * npu-runtime benchmark.
 *
 *   npu-runtime-bench [--elf PATH] [--carrier BLOB] [--runs N] [--threads T]
 *                     [--recreate] [--csv FILE] [--json]
 *
 * Reports, for one kernel:
 *   - the one-off setup split (patch / graph create / argument query / init)
 *   - the first (cold) invocation
 *   - N back-to-back invocations: min / median / mean / p90 of the total, plus
 *     the fence wait (device time) and the host-side session cost
 *   - achieved invocations/s and elements/s
 *
 * --recreate repeats npurt_shutdown() + npurt_kernel_create() between runs,
 * which reproduces what a caller that does not reuse the graph session pays
 * (the npunlock linux probe / infer_engine behaviour).
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npu_runtime.h"


typedef struct sample {
  uint64_t total;
  uint64_t wait;
  uint64_t session;
  uint64_t submit;
  uint64_t prepare;
  uint64_t teardown;
} sample;

static int compare_u64(const void *left, const void *right) {
  uint64_t a = *(const uint64_t *)left;
  uint64_t b = *(const uint64_t *)right;
  return a < b ? -1 : (a > b ? 1 : 0);
}

static uint64_t percentile(uint64_t *values, size_t count, double fraction) {
  size_t index;
  if (count == 0) {
    return 0;
  }
  index = (size_t)(fraction * (double)(count - 1) + 0.5);
  return values[index];
}

static uint64_t mean(const uint64_t *values, size_t count) {
  uint64_t total = 0;
  size_t index;
  for (index = 0; index < count; ++index) {
    total += values[index];
  }
  return count == 0 ? 0 : total / count;
}

static uint8_t *read_file(const char *path, size_t *size) {
  FILE *stream = fopen(path, "rb");
  long length;
  uint8_t *data;
  if (stream == NULL) {
    return NULL;
  }
  fseek(stream, 0, SEEK_END);
  length = ftell(stream);
  fseek(stream, 0, SEEK_SET);
  if (length <= 0) {
    fclose(stream);
    return NULL;
  }
  data = (uint8_t *)malloc((size_t)length);
  if (fread(data, 1, (size_t)length, stream) != (size_t)length) {
    free(data);
    fclose(stream);
    return NULL;
  }
  fclose(stream);
  *size = (size_t)length;
  return data;
}

typedef struct thread_context {
  npurt_kernel *kernel;
  const void *input;
  size_t input_size;
  void *output;
  size_t output_size;
  int runs;
  int failures;
  uint64_t last_output_checksum;
} thread_context;

static uint64_t checksum(const void *data, size_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  uint64_t hash = 1469598103934665603ull;
  size_t index;
  for (index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
  return hash;
}

static void *thread_main(void *argument) {
  thread_context *context = (thread_context *)argument;
  int index;
  for (index = 0; index < context->runs; ++index) {
    npurt_run_request request;
    npurt_diagnostic diagnostic;
    memset(&request, 0, sizeof(request));
    npurt_diagnostic_init(&diagnostic);
    request.input = context->input;
    request.input_size = context->input_size;
    request.output = context->output;
    request.output_size = context->output_size;
    if (npurt_run(context->kernel, &request, NULL, &diagnostic) != NPURT_OK) {
      ++context->failures;
    }
  }
  context->last_output_checksum = checksum(context->output, context->output_size);
  return NULL;
}

int main(int argc, char **argv) {
  const char *elf_path = "/home/asdf/projects/npunlock-work/our-add1-fp16.elf";
  const char *blob_path = NULL;
  const char *carrier = NULL;
  const char *csv_path = NULL;
  int runs = 200;
  int threads = 0;
  int recreate = 0;
  uint64_t probe_timeout_ns = 0;
  int json = 0;
  int index;
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel *kernel = NULL;
  npurt_kernel_info info;
  npurt_status status;
  uint8_t *elf;
  size_t elf_size = 0;
  uint16_t *input = NULL;
  uint16_t *output = NULL;
  size_t input_bytes = 0;
  size_t output_bytes = 0;
  sample *samples = NULL;
  uint64_t *totals = NULL;
  uint64_t first_total = 0;
  int collected = 0;

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--elf") == 0 && index + 1 < argc) {
      elf_path = argv[++index];
    } else if (strcmp(argv[index], "--blob") == 0 && index + 1 < argc) {
      blob_path = argv[++index];
    } else if (strcmp(argv[index], "--carrier") == 0 && index + 1 < argc) {
      carrier = argv[++index];
    } else if (strcmp(argv[index], "--csv") == 0 && index + 1 < argc) {
      csv_path = argv[++index];
    } else if (strcmp(argv[index], "--runs") == 0 && index + 1 < argc) {
      runs = atoi(argv[++index]);
    } else if (strcmp(argv[index], "--threads") == 0 && index + 1 < argc) {
      threads = atoi(argv[++index]);
    } else if (strcmp(argv[index], "--timeout-us") == 0 && index + 1 < argc) {
      probe_timeout_ns = (uint64_t)strtoull(argv[++index], NULL, 10) * 1000ull;
    } else if (strcmp(argv[index], "--recreate") == 0) {
      recreate = 1;
    } else if (strcmp(argv[index], "--json") == 0) {
      json = 1;
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[index]);
      return 2;
    }
  }

  if (blob_path != NULL) {
    elf = read_file(blob_path, &elf_size);
    if (elf == NULL) {
      fprintf(stderr, "cannot read graph blob %s\n", blob_path);
      return 1;
    }
  } else {
    elf = read_file(elf_path, &elf_size);
    if (elf == NULL) {
      fprintf(stderr, "cannot read ELF %s\n", elf_path);
      return 1;
    }
  }
  npurt_config_defaults(&config);
  if (carrier != NULL) {
    snprintf(config.carrier_blob, sizeof(config.carrier_blob), "%s", carrier);
  }
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/bench");
  config.verbosity = 0;
  npurt_diagnostic_init(&diagnostic);

  {
    uint64_t warm_start = 0;
    uint64_t warm_ns;
    warm_start = 0;
    {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      warm_start = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    }
    status = npurt_configure(&config, &diagnostic);
    if (status == NPURT_OK) {
      status = npurt_warm(&diagnostic);
    }
    {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      warm_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    }
    if (status != NPURT_OK) {
      fprintf(stderr, "warm failed: %s %s\n", npurt_status_name(status), diagnostic.message);
      return 1;
    }
    if (!json) {
      printf("warm (loader + device + context + queue group): %.3f ms\n",
             (double)(warm_ns - warm_start) / 1e6);
    }
  }

  status = blob_path != NULL ? npurt_kernel_create_from_blob(elf, elf_size, &kernel, &diagnostic)
                            : npurt_kernel_create(elf, elf_size, &kernel, &diagnostic);
  if (status != NPURT_OK) {
    fprintf(stderr, "kernel create failed: %s %s\n", npurt_status_name(status), diagnostic.message);
    return 1;
  }
  npurt_kernel_info_get(kernel, &info);
  {
    uint32_t tensor;
    for (tensor = 0; tensor < info.tensor_count; ++tensor) {
      if (info.tensors[tensor].is_input) {
        input_bytes = (size_t)info.tensors[tensor].byte_size;
      } else {
        output_bytes = (size_t)info.tensors[tensor].byte_size;
      }
    }
  }
  if (input_bytes == 0 || output_bytes == 0) {
    fprintf(stderr, "unsupported tensor sizes (%zu in / %zu out)\n", input_bytes, output_bytes);
    return 1;
  }
  input = (uint16_t *)calloc((input_bytes + 1) / 2, sizeof(uint16_t));
  output = (uint16_t *)calloc((output_bytes + 1) / 2, sizeof(uint16_t));
  {
    size_t element;
    for (element = 0; element < input_bytes / 2; ++element) {
      input[element] = (uint16_t)(0x3c00u + element); /* arbitrary deterministic pattern */
    }
  }

  if (!json) {
    printf("kernel: %s (%zu bytes)%s\n", blob_path != NULL ? blob_path : elf_path, elf_size,
           blob_path != NULL ? "  [prebuilt graph blob]" : "");
    printf("  blob %llu bytes, %u patch target(s), %u tensor(s)\n",
           (unsigned long long)info.blob_bytes, info.patch_target_count, info.tensor_count);
    printf("  setup: patch %.3f ms | graph create %.3f ms | arg query %.3f ms | init %.3f ms | "
           "total %.3f ms\n",
           (double)info.patch_ns / 1e6, (double)info.graph_create_ns / 1e6,
           (double)info.query_ns / 1e6, (double)info.init_ns / 1e6, (double)info.setup_ns / 1e6);
    printf("  tensors: input %zu bytes, output %zu bytes%s\n", input_bytes, output_bytes,
           recreate ? "  [--recreate: no session reuse]" : "");
  }

  if (threads > 1) {
    pthread_t *workers = (pthread_t *)calloc((size_t)threads, sizeof(*workers));
    thread_context *contexts = (thread_context *)calloc((size_t)threads, sizeof(*contexts));
    uint64_t start;
    uint64_t elapsed;
    int total_failures = 0;
    struct timespec ts;
    for (index = 0; index < threads; ++index) {
      contexts[index].kernel = kernel;
      contexts[index].input = input;
      contexts[index].input_size = input_bytes;
      contexts[index].output = output;
      contexts[index].output_size = output_bytes;
      contexts[index].runs = runs;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    for (index = 0; index < threads; ++index) {
      pthread_create(&workers[index], NULL, thread_main, &contexts[index]);
    }
    for (index = 0; index < threads; ++index) {
      pthread_join(workers[index], NULL);
      total_failures += contexts[index].failures;
    }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    elapsed = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec - start;
    printf("  %d threads x %d runs: %d failures, %.3f ms wall, %.3f ms per call\n", threads, runs,
           total_failures, (double)elapsed / 1e6,
           (double)elapsed / 1e6 / (double)(threads * runs));
    free(contexts);
    free(workers);
    npurt_shutdown();
    free(elf);
    free(input);
    free(output);
    return total_failures == 0 ? 0 : 1;
  }

  samples = (sample *)calloc((size_t)runs + 1, sizeof(*samples));
  totals = (uint64_t *)calloc((size_t)runs, sizeof(*totals));
  if (samples == NULL || totals == NULL) {
    return 1;
  }
  for (index = 0; index < runs; ++index) {
    npurt_run_request request;
    npurt_timing timing;
    memset(&request, 0, sizeof(request));
    memset(&timing, 0, sizeof(timing));
    request.input = input;
    request.input_size = input_bytes;
    request.output = output;
    request.output_size = output_bytes;
    if (index == 0 && probe_timeout_ns != 0) {
      request.timeout_ns = probe_timeout_ns;
    }
    if (recreate && blob_path == NULL) {
      uint64_t create_start = 0;
      uint64_t create_ns;
      struct timespec ts;
      npurt_shutdown();
      clock_gettime(CLOCK_MONOTONIC, &ts);
      create_start = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
      status = npurt_kernel_create(elf, elf_size, &kernel, &diagnostic);
      clock_gettime(CLOCK_MONOTONIC, &ts);
      create_ns = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
      if (status != NPURT_OK) {
        fprintf(stderr, "recreate failed: %s %s\n", npurt_status_name(status), diagnostic.message);
        return 1;
      }
      status = npurt_run(kernel, &request, &timing, &diagnostic);
      timing.total_ns += create_ns - create_start;
      timing.prepare_ns += create_ns - create_start;
      samples[index].prepare = create_ns - create_start;
    } else {
      status = npurt_run(kernel, &request, &timing, &diagnostic);
      samples[index].prepare = timing.prepare_ns;
    }
    if (status != NPURT_OK) {
      if (index == 0 && probe_timeout_ns != 0) {
        printf("  timeout probe: run 0 with a %llu us fence timeout -> %s (%s)\n",
               (unsigned long long)(probe_timeout_ns / 1000), npurt_status_name(status),
               diagnostic.message);
        printf("  timeout probe: continuing with the default timeout to test recovery\n");
        continue;
      }
      fprintf(stderr, "run %d failed: %s %s\n", index, npurt_status_name(status),
              diagnostic.message);
      return 1;
    }
    samples[collected].total = timing.total_ns;
    samples[collected].wait = timing.wait_ns;
    samples[collected].session = timing.session_ns;
    samples[collected].submit = timing.submit_ns;
    samples[collected].teardown = timing.teardown_ns;
    totals[collected] = timing.total_ns;
    if (collected == 0) {
      first_total = timing.total_ns;
    }
    ++collected;
  }
  if (collected == 0) {
    fprintf(stderr, "no successful invocation was collected\n");
    return 1;
  }
  qsort(totals, (size_t)collected, sizeof(uint64_t), compare_u64);

  {
    uint64_t *waits = (uint64_t *)calloc((size_t)collected, sizeof(uint64_t));
    double elements = (double)(input_bytes / 2);
    double calls_per_second = 1e9 / (double)totals[collected / 2];
    for (index = 0; index < collected; ++index) {
      waits[index] = samples[index].wait;
    }
    qsort(waits, (size_t)collected, sizeof(uint64_t), compare_u64);
    if (json) {
      printf("{\"elf\":\"%s\",\"runs\":%d,\"first_ns\":%llu,\"min_ns\":%llu,\"median_ns\":%llu,"
             "\"mean_ns\":%llu,\"p90_ns\":%llu,\"wait_median_ns\":%llu,\"session_median_ns\":%llu,"
             "\"submit_median_ns\":%llu,\"elements\":%.0f,\"calls_per_s\":%.1f,"
             "\"elements_per_s\":%.1f,\"recreate\":%d}\n",
             blob_path != NULL ? blob_path : elf_path, runs, (unsigned long long)first_total, (unsigned long long)totals[0],
             (unsigned long long)totals[runs / 2], (unsigned long long)mean(totals, (size_t)runs),
             (unsigned long long)percentile(totals, (size_t)runs, 0.9),
             (unsigned long long)percentile(waits, (size_t)runs, 0.5),
             (unsigned long long)percentile(totals, (size_t)runs, 0.5) /* placeholder */,
             (unsigned long long)samples[runs / 2].submit, elements, calls_per_second,
             calls_per_second * elements, recreate);
    } else {
      printf("\n%d invocations, one kernel session%s\n", collected,
             recreate ? " (recreated per call)" : "");
      printf("  first (cold) call : %8.3f ms\n", (double)first_total / 1e6);
      printf("  min               : %8.3f ms\n", (double)totals[0] / 1e6);
      printf("  median            : %8.3f ms\n", (double)totals[collected / 2] / 1e6);
      printf("  mean              : %8.3f ms\n", (double)mean(totals, (size_t)collected) / 1e6);
      printf("  p90               : %8.3f ms\n",
             (double)percentile(totals, (size_t)collected, 0.9) / 1e6);
      printf("  breakdown (median): fence wait %.3f ms | command session %.3f ms | submit %.3f ms | "
             "teardown %.3f ms | host prepare %.3f ms\n",
             (double)percentile(waits, (size_t)collected, 0.5) / 1e6,
             (double)samples[collected / 2].session / 1e6,
             (double)samples[collected / 2].submit / 1e6,
             (double)samples[collected / 2].teardown / 1e6,
             (double)samples[collected / 2].prepare / 1e6);
      printf("  throughput        : %.1f invocations/s, %.1f elements/s (%d elements/tile)\n",
             calls_per_second, calls_per_second * elements, (int)elements);
    }
    free(waits);
  }

  if (csv_path != NULL) {
    FILE *csv = fopen(csv_path, "wb");
    if (csv != NULL) {
      fprintf(csv, "run,total_ns,prepare_ns,session_ns,submit_ns,wait_ns,teardown_ns\n");
      for (index = 0; index < runs; ++index) {
        fprintf(csv, "%d,%llu,%llu,%llu,%llu,%llu,%llu\n", index,
                (unsigned long long)samples[index].total,
                (unsigned long long)samples[index].prepare,
                (unsigned long long)samples[index].session,
                (unsigned long long)samples[index].submit,
                (unsigned long long)samples[index].wait,
                (unsigned long long)samples[index].teardown);
      }
      fclose(csv);
      if (!json) {
        printf("  per-call samples written to %s\n", csv_path);
      }
    }
  }

  npurt_shutdown();
  free(elf);
  free(input);
  free(output);
  free(samples);
  free(totals);
  return 0;
}
