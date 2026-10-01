/* kernel-lib: arbitrary-shape NPU execution probe.
 *
 * npunlock's own `npunlock-linux-probe` is pinned to the fixed 1x16 / 32-byte
 * fp16 porting-probe contract (src/linux_probe/main.c rejects any other tensor
 * size), so it cannot execute a 1x32 carrier or any future model-sized
 * carrier.  This probe reuses the very same execution engine
 * (npunlock/src/linux_probe/infer_engine.c, compiled from the read-only
 * npunlock tree) with a thin command line that accepts any tensor size and
 * forwards up to NPUNLOCK_INFER_MAX_ARGUMENTS named input files.
 *
 * usage: klib_probe --graph G.blob --input-0 FILE [--input-1 FILE ...]
 *                   [--output OUT.bin] [--report REPORT.json] [--quiet]
 *
 * Exit status 0 when the execution stage completed (regardless of the oracle;
 * the caller compares bytes), 1 on a driver/engine failure, 2 on bad usage.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "infer_engine.h"

#define MAX_INPUT_FILES 8

static uint8_t *read_file(const char *path, size_t *size_out) {
  FILE *handle = fopen(path, "rb");
  uint8_t *data;
  long size;
  if (handle == NULL) {
    return NULL;
  }
  if (fseek(handle, 0, SEEK_END) != 0) {
    fclose(handle);
    return NULL;
  }
  size = ftell(handle);
  if (size < 0) {
    fclose(handle);
    return NULL;
  }
  rewind(handle);
  data = (uint8_t *)malloc((size_t)size + 1u);
  if (data == NULL) {
    fclose(handle);
    return NULL;
  }
  if (size > 0 && fread(data, 1, (size_t)size, handle) != (size_t)size) {
    free(data);
    fclose(handle);
    return NULL;
  }
  fclose(handle);
  *size_out = (size_t)size;
  return data;
}

static int write_file(const char *path, const uint8_t *data, size_t size) {
  FILE *handle = fopen(path, "wb");
  if (handle == NULL) {
    return 0;
  }
  if (size > 0 && fwrite(data, 1, size, handle) != size) {
    fclose(handle);
    return 0;
  }
  fclose(handle);
  return 1;
}

static const char *stage_state(const npunlock_infer_result *result, int stage) {
  if (result->status == NPUNLOCK_INFER_OK) {
    return "passed";
  }
  if ((int)result->stage > stage) {
    return "passed";
  }
  if ((int)result->stage == stage) {
    return "failed";
  }
  return "not-run";
}

int main(int argc, char **argv) {
  const char *graph_path = NULL;
  const char *input_paths[MAX_INPUT_FILES] = {0};
  size_t input_count = 0;
  const char *output_path = NULL;
  const char *report_path = NULL;
  int quiet = 0;
  uint8_t *graph = NULL;
  uint8_t *inputs[MAX_INPUT_FILES] = {0};
  size_t input_sizes[MAX_INPUT_FILES] = {0};
  size_t graph_size = 0;
  npunlock_infer_request request;
  npunlock_infer_result result;
  FILE *report;
  int exit_code = 1;
  size_t index;

  for (int arg = 1; arg < argc; ++arg) {
    if (strcmp(argv[arg], "--graph") == 0 && arg + 1 < argc) {
      graph_path = argv[++arg];
    } else if (strcmp(argv[arg], "--output") == 0 && arg + 1 < argc) {
      output_path = argv[++arg];
    } else if (strcmp(argv[arg], "--report") == 0 && arg + 1 < argc) {
      report_path = argv[++arg];
    } else if (strcmp(argv[arg], "--quiet") == 0) {
      quiet = 1;
    } else if (strncmp(argv[arg], "--input-", 8) == 0 && arg + 1 < argc) {
      if (input_count >= MAX_INPUT_FILES) {
        fprintf(stderr, "too many input files\n");
        return 2;
      }
      input_paths[input_count++] = argv[++arg];
    } else {
      fprintf(stderr, "unknown or incomplete option: %s\n", argv[arg]);
      return 2;
    }
  }
  if (graph_path == NULL || input_count == 0) {
    fprintf(stderr, "usage: klib_probe --graph G.blob --input-0 FILE "
                    "[--input-N FILE ...] [--output OUT.bin] [--report R.json]\n");
    return 2;
  }

  graph = read_file(graph_path, &graph_size);
  if (graph == NULL || graph_size == 0) {
    fprintf(stderr, "cannot read graph %s\n", graph_path);
    return 2;
  }
  for (index = 0; index < input_count; ++index) {
    inputs[index] = read_file(input_paths[index], &input_sizes[index]);
    if (inputs[index] == NULL) {
      fprintf(stderr, "cannot read input %s\n", input_paths[index]);
      return 2;
    }
  }

  memset(&request, 0, sizeof(request));
  memset(&result, 0, sizeof(result));
  request.driver_index = NPUNLOCK_INFER_AUTO_INDEX;
  request.device_index = NPUNLOCK_INFER_AUTO_INDEX;
  request.graph = graph;
  request.graph_size = graph_size;
  request.input_count = input_count;
  for (index = 0; index < input_count; ++index) {
    request.inputs[index].argument_index = (uint32_t)index;
    request.inputs[index].data = inputs[index];
    request.inputs[index].data_size = input_sizes[index];
  }

  npunlock_infer_execute(&request, &result);

  if (!quiet) {
    printf("status=%d stage=%s level_zero_result=0x%08x outputs=%zu\n",
           (int)result.status, npunlock_infer_stage_name(result.stage),
           result.driver_result, result.output_count);
    if (result.diagnostic != NULL) {
      printf("diagnostic: %s\n", result.diagnostic);
    }
  }
  if (result.status == NPUNLOCK_INFER_OK && result.output_count >= 1) {
    for (index = 0; index < result.output_count; ++index) {
      if (!quiet) {
        printf("output[%zu] argument=%u bytes=%zu\n", index,
               result.outputs[index].argument_index, result.outputs[index].data_size);
      }
    }
    if (output_path != NULL) {
      if (!write_file(output_path, result.outputs[0].data, result.outputs[0].data_size)) {
        fprintf(stderr, "cannot write output %s\n", output_path);
        goto done;
      }
    }
    exit_code = 0;
  }

done:
  if (report_path != NULL) {
    report = fopen(report_path, "wb");
    if (report != NULL) {
      fprintf(report,
              "{\n  \"schema\": \"kernel-lib.klib-probe.v1\",\n  \"stages\": {\n"
              "    \"loader\": \"%s\",\n    \"device_selection\": \"%s\",\n"
              "    \"graph_extension\": \"%s\",\n    \"graph_create\": \"%s\",\n"
              "    \"initialization\": \"%s\",\n    \"execution\": \"%s\",\n"
              "    \"complete\": \"%s\"\n  },\n"
              "  \"result\": {\"status\": %d, \"stage\": \"%s\", "
              "\"level_zero_result\": \"0x%08x\", \"output_count\": %zu},\n"
              "  \"output\": {\"argument_index\": %u, \"bytes\": %zu}\n}\n",
              stage_state(&result, NPUNLOCK_INFER_STAGE_LOADER),
              stage_state(&result, NPUNLOCK_INFER_STAGE_DEVICE),
              stage_state(&result, NPUNLOCK_INFER_STAGE_GRAPH_EXTENSION),
              stage_state(&result, NPUNLOCK_INFER_STAGE_GRAPH_CREATE),
              stage_state(&result, NPUNLOCK_INFER_STAGE_INITIALIZATION),
              stage_state(&result, NPUNLOCK_INFER_STAGE_EXECUTION),
              stage_state(&result, NPUNLOCK_INFER_STAGE_COMPLETE),
              (int)result.status, npunlock_infer_stage_name(result.stage),
              result.driver_result, result.output_count,
              result.output_count ? result.outputs[0].argument_index : 0u,
              result.output_count ? result.outputs[0].data_size : (size_t)0);
      fclose(report);
    }
  }
  npunlock_infer_result_release(&result);
  free(graph);
  for (index = 0; index < input_count; ++index) {
    free(inputs[index]);
  }
  return exit_code;
}
