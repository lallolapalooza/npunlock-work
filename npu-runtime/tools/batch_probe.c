/*
 * Batch/pipelining probe.
 *
 *   npu-runtime-batch [--elf PATH] [--carrier BLOB] [--width N] [--rounds R]
 *
 * `npurt_run` waits for its own completion, so a serialized caller pays the
 * full submit-to-fence round trip (~0.96 ms on the 1x16 carrier). Cloning the
 * session N times and submitting all N executions in one command list lets the
 * invocations overlap: one submit, one wait.
 *
 * The probe checks first that batching is *correct* (every clone receives a
 * different input tile and its output must equal the serialized reference for
 * that tile, bit for bit), then reports both latencies.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npu_runtime.h"

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

int main(int argc, char **argv) {
  const char *elf_path = "/home/asdf/projects/npunlock-work/our-add1-fp16.elf";
  const char *carrier = NULL;
  int width = 8;
  int rounds = 3;
  int index;
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel *base = NULL;
  npurt_kernel **kernels;
  npurt_run_request *requests;
  uint8_t **inputs;
  uint8_t **outputs;
  uint8_t **reference;
  npurt_kernel_info info;
  uint8_t *elf;
  size_t elf_size = 0;
  size_t input_bytes = 0;
  size_t output_bytes = 0;
  int mismatches = 0;
  double serialized_ns = 0.0;
  double batched_ns = 0.0;

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--elf") == 0 && index + 1 < argc) {
      elf_path = argv[++index];
    } else if (strcmp(argv[index], "--carrier") == 0 && index + 1 < argc) {
      carrier = argv[++index];
    } else if (strcmp(argv[index], "--width") == 0 && index + 1 < argc) {
      width = atoi(argv[++index]);
    } else if (strcmp(argv[index], "--rounds") == 0 && index + 1 < argc) {
      rounds = atoi(argv[++index]);
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[index]);
      return 2;
    }
  }
  if (width < 1 || width > 64) {
    fprintf(stderr, "--width must be 1..64\n");
    return 2;
  }

  npurt_config_defaults(&config);
  if (carrier != NULL) {
    snprintf(config.carrier_blob, sizeof(config.carrier_blob), "%s", carrier);
  }
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/batch");
  config.verbosity = 0;
  npurt_diagnostic_init(&diagnostic);
  if (npurt_configure(&config, &diagnostic) != NPURT_OK || npurt_warm(&diagnostic) != NPURT_OK) {
    fprintf(stderr, "init: %s\n", diagnostic.message);
    return 1;
  }
  elf = read_file(elf_path, &elf_size);
  if (elf == NULL) {
    fprintf(stderr, "cannot read %s\n", elf_path);
    return 1;
  }
  if (npurt_kernel_create(elf, elf_size, &base, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "kernel create: %s\n", diagnostic.message);
    return 1;
  }
  npurt_kernel_info_get(base, &info);
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
  printf("%s: %zu input bytes, %zu output bytes, batch width %d\n", elf_path, input_bytes,
         output_bytes, width);

  kernels = (npurt_kernel **)calloc((size_t)width, sizeof(*kernels));
  requests = (npurt_run_request *)calloc((size_t)width, sizeof(*requests));
  inputs = (uint8_t **)calloc((size_t)width, sizeof(*inputs));
  outputs = (uint8_t **)calloc((size_t)width, sizeof(*outputs));
  reference = (uint8_t **)calloc((size_t)width, sizeof(*reference));
  kernels[0] = base;
  for (index = 1; index < width; ++index) {
    if (npurt_kernel_clone(base, &kernels[index], &diagnostic) != NPURT_OK) {
      fprintf(stderr, "clone %d: %s\n", index, diagnostic.message);
      return 1;
    }
  }
  for (index = 0; index < width; ++index) {
    size_t element;
    inputs[index] = (uint8_t *)calloc(1, input_bytes);
    outputs[index] = (uint8_t *)calloc(1, output_bytes);
    reference[index] = (uint8_t *)calloc(1, output_bytes);
    /* a distinct, non-trivial pattern per slot */
    for (element = 0; element < input_bytes / 2; ++element) {
      ((uint16_t *)inputs[index])[element] = (uint16_t)(0x3c00u + index * 7u + element);
    }
    memset(&requests[index], 0, sizeof(requests[index]));
    requests[index].input = inputs[index];
    requests[index].input_size = input_bytes;
    requests[index].output = outputs[index];
    requests[index].output_size = output_bytes;
  }

  /* serialized reference: each handle, its own call */
  for (index = 0; index < width; ++index) {
    npurt_run_request request = requests[index];
    npurt_timing timing;
    request.output = reference[index];
    if (npurt_run(kernels[index], &request, &timing, &diagnostic) != NPURT_OK) {
      fprintf(stderr, "reference run %d: %s\n", index, diagnostic.message);
      return 1;
    }
    serialized_ns += (double)timing.total_ns;
  }

  /* batched: one submission for all handles */
  {
    npurt_timing timing;
    if (npurt_run_batch(kernels, requests, (size_t)width, &timing, &diagnostic) != NPURT_OK) {
      fprintf(stderr, "batch: %s\n", diagnostic.message);
      return 1;
    }
    batched_ns = (double)timing.total_ns;
    for (index = 0; index < width; ++index) {
      if (memcmp(outputs[index], reference[index], output_bytes) != 0) {
        printf("  [FAIL] batch slot %d differs from the serialized result\n", index);
        ++mismatches;
      }
    }
  }
  if (mismatches == 0) {
    printf("  [ ok ] all %d batch results are bit-identical to the serialized ones\n", width);
  }

  /* repeat both paths to smooth out the first-call effects */
  serialized_ns = 0.0;
  batched_ns = 0.0;
  {
    int round;
    for (round = 0; round < rounds; ++round) {
      npurt_timing timing;
      for (index = 0; index < width; ++index) {
        npurt_run_request request = requests[index];
        request.output = reference[index];
        if (npurt_run(kernels[index], &request, &timing, &diagnostic) != NPURT_OK) {
          fprintf(stderr, "serialized run failed: %s\n", diagnostic.message);
          return 1;
        }
        serialized_ns += (double)timing.total_ns;
      }
      if (npurt_run_batch(kernels, requests, (size_t)width, &timing, &diagnostic) != NPURT_OK) {
        fprintf(stderr, "batch failed: %s\n", diagnostic.message);
        return 1;
      }
      batched_ns += (double)timing.total_ns;
    }
  }
  serialized_ns /= (double)(rounds * width);
  batched_ns /= (double)(rounds * width);
  printf("  serialized : %.3f ms/call (%.0f calls/s)\n", serialized_ns / 1e6, 1e9 / serialized_ns);
  printf("  batched    : %.3f ms/call (%.0f calls/s)  [%.2fx]\n", batched_ns / 1e6, 1e9 / batched_ns,
         serialized_ns / batched_ns);
  printf("  elements/s : serialized %.0f, batched %.0f (%.0f elements/tile)\n",
         (input_bytes / 2) * (1e9 / serialized_ns), (input_bytes / 2) * (1e9 / batched_ns),
         (double)(input_bytes / 2));

  for (index = 0; index < width; ++index) {
    free(inputs[index]);
    free(outputs[index]);
    free(reference[index]);
  }
  for (index = 1; index < width; ++index) {
    npurt_kernel_free(kernels[index]);
  }
  npurt_shutdown();
  free(kernels);
  free(requests);
  free(inputs);
  free(outputs);
  free(reference);
  free(elf);
  return mismatches == 0 ? 0 : 1;
}
