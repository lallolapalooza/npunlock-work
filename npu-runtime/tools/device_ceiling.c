/*
 * Device ceiling probe (internal API).
 *
 *   npu-runtime-ceiling [ELF] [--kmax 32]
 *
 * Answers one question: is the ~0.8 ms fence wait of a single invocation paid
 * per *submission* or per *graph execution*? It issues K graph executions in
 * ONE command list (K appends, one queue submit, one fence wait) and reports
 * the marginal cost of an extra invocation. That number is the ceiling a
 * batched/pipelined caller (for example a ggml backend that queues several
 * tiles) could reach.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npurt_internal.h"

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
  const char *elf_path = argc > 1 && argv[1][0] != '-' ? argv[1]
                                                       : "/home/asdf/projects/npunlock-work/our-add1-fp16.elf";
  int kmax = 32;
  int index;
  const char *carrier = NULL;
  const char *blob_path = NULL;
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel *kernel = NULL;
  npurt_kernel_info info;
  npurt_state *state;
  uint8_t *elf;
  size_t elf_size = 0;
  uint8_t *input;
  uint8_t *output;
  size_t input_bytes = 0;
  size_t output_bytes = 0;
  int k;

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--kmax") == 0 && index + 1 < argc) {
      kmax = atoi(argv[++index]);
    } else if (strcmp(argv[index], "--carrier") == 0 && index + 1 < argc) {
      carrier = argv[++index];
    } else if (strcmp(argv[index], "--blob") == 0 && index + 1 < argc) {
      blob_path = argv[++index];
    } else if (argv[index][0] != '-') {
      elf_path = argv[index];
    }
  }

  npurt_config_defaults(&config);
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/ceiling");
  config.verbosity = 0;
  if (carrier != NULL) {
    snprintf(config.carrier_blob, sizeof(config.carrier_blob), "%s", carrier);
  }
  npurt_diagnostic_init(&diagnostic);
  if (npurt_configure(&config, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "configure failed: %s\n", diagnostic.message);
    return 1;
  }
  elf = read_file(blob_path != NULL ? blob_path : elf_path, &elf_size);
  if (elf == NULL) {
    fprintf(stderr, "cannot read %s\n", elf_path);
    return 1;
  }
  if (blob_path != NULL
          ? npurt_kernel_create_from_blob(elf, elf_size, &kernel, &diagnostic) != NPURT_OK
          : npurt_kernel_create(elf, elf_size, &kernel, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "kernel create failed: %s\n", diagnostic.message);
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
  input = (uint8_t *)calloc(1, input_bytes);
  output = (uint8_t *)calloc(1, output_bytes);
  if (input == NULL || output == NULL) {
    return 1;
  }
  /* one normal invocation so that the driver's lazy resources exist */
  {
    npurt_run_request request;
    memset(&request, 0, sizeof(request));
    request.input = input;
    request.input_size = input_bytes;
    request.output = output;
    request.output_size = output_bytes;
    if (npurt_run(kernel, &request, NULL, &diagnostic) != NPURT_OK) {
      fprintf(stderr, "warm-up run failed: %s\n", diagnostic.message);
      return 1;
    }
  }

  state = npurt_state_get();
  printf("%s: input %zu bytes, output %zu bytes (%zu elements per graph execution)\n", elf_path,
         input_bytes, output_bytes, input_bytes / 2);
  printf("%5s %12s %14s %14s %14s\n", "K", "total_us", "per_call_us", "append_us", "wait_us");
  for (k = 1; k <= kmax; k *= 2) {
    int repeat;
    uint64_t best_total = 0;
    uint64_t best_wait = 0;
    uint64_t best_session = 0;
    for (repeat = 0; repeat < 3; ++repeat) {
      npunlock_ze_command_list_desc list_desc;
      npunlock_ze_fence_desc fence_desc;
      npunlock_ze_command_list_handle list = NULL;
      npunlock_ze_fence_handle fence = NULL;
      uint64_t start;
      uint64_t appended;
      uint64_t submitted;
      uint64_t finished;
      uint32_t code;
      int execution;
      memset(&list_desc, 0, sizeof(list_desc));
      memset(&fence_desc, 0, sizeof(fence_desc));
      list_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
      list_desc.commandQueueGroupOrdinal = state->queue_ordinal;
      fence_desc.stype = NPUNLOCK_ZE_STRUCTURE_TYPE_FENCE_DESC;
      start = npurt_now_ns();
      code = state->ze.command_list_create(state->context, state->device, &list_desc, &list);
      if (code != NPUNLOCK_ZE_SUCCESS) {
        fprintf(stderr, "zeCommandListCreate failed (0x%08x)\n", code);
        return 1;
      }
      for (execution = 0; execution < k; ++execution) {
        code = state->graph.append_execute(list, kernel->handle, NULL, NULL, 0, NULL);
        if (code != NPUNLOCK_ZE_SUCCESS) {
          fprintf(stderr, "append_execute failed (0x%08x)\n", code);
          return 1;
        }
      }
      code = state->ze.command_list_close(list);
      appended = npurt_now_ns();
      code = state->ze.fence_create(state->queue, &fence_desc, &fence);
      code = state->ze.command_queue_execute(state->queue, 1, &list, fence);
      submitted = npurt_now_ns();
      if (code != NPUNLOCK_ZE_SUCCESS) {
        fprintf(stderr, "execute failed (0x%08x)\n", code);
        return 1;
      }
      code = state->ze.fence_synchronize(fence, 5000000000ull);
      finished = npurt_now_ns();
      if (code != NPUNLOCK_ZE_SUCCESS) {
        fprintf(stderr, "fence sync failed (0x%08x)\n", code);
        return 1;
      }
      if (repeat == 0 || finished - start < best_total) {
        best_total = finished - start;
        best_session = appended - start;
        best_wait = finished - submitted;
      }
      state->ze.fence_destroy(fence);
      state->ze.command_list_destroy(list);
      if (repeat + 1 < 3) {
        (void)submitted;
      }
    }
    printf("%5d %12.1f %14.1f %14.1f %14.1f\n", k, (double)best_total / 1000.0,
           (double)best_total / 1000.0 / (double)k, (double)best_session / 1000.0,
           (double)best_wait / 1000.0);
    fflush(stdout);
  }
  free(input);
  free(output);
  free(elf);
  npurt_shutdown();
  return 0;
}
