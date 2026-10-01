/*
 * Session lifecycle probe (internal API).
 *
 *   npu-runtime-session [ELF]
 *
 * Walks the lifecycle states that matter for a long-lived caller and reports
 * where the device stack tolerates them:
 *   1. create + run, drop the kernel, create + run again (same session)
 *   2. npurt_shutdown() + npurt_warm() + create + run (new session)
 *   3. repeat (2) twice
 * Every step prints to stderr before it runs, so a crash identifies the step.
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

static int execute_once(npurt_kernel *kernel, const char *label) {
  npurt_diagnostic diagnostic;
  npurt_run_request request;
  npurt_kernel_info info;
  uint8_t *input;
  uint8_t *output;
  size_t input_bytes = 0;
  size_t output_bytes = 0;
  int status;
  uint32_t tensor;
  npurt_diagnostic_init(&diagnostic);
  npurt_kernel_info_get(kernel, &info);
  for (tensor = 0; tensor < info.tensor_count; ++tensor) {
    if (info.tensors[tensor].is_input) {
      input_bytes = (size_t)info.tensors[tensor].byte_size;
    } else {
      output_bytes = (size_t)info.tensors[tensor].byte_size;
    }
  }
  input = (uint8_t *)calloc(1, input_bytes);
  output = (uint8_t *)calloc(1, output_bytes);
  memset(&request, 0, sizeof(request));
  request.input = input;
  request.input_size = input_bytes;
  request.output = output;
  request.output_size = output_bytes;
  fprintf(stderr, "  run [%s] ... ", label);
  fflush(stderr);
  status = npurt_run(kernel, &request, NULL, &diagnostic);
  fprintf(stderr, "%s\n", status == NPURT_OK ? "ok" : npurt_status_name(status));
  fflush(stderr);
  free(input);
  free(output);
  return status == NPURT_OK ? 0 : 1;
}

int main(int argc, char **argv) {
  const char *elf_path =
      argc > 1 && argv[1][0] != '-' ? argv[1] : "/home/asdf/projects/npunlock-work/our-add1-fp16.elf";
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel *kernel = NULL;
  uint8_t *elf;
  size_t elf_size = 0;
  int cycle;

  npurt_config_defaults(&config);
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/session");
  config.verbosity = 0;
  npurt_diagnostic_init(&diagnostic);
  fprintf(stderr, "configure ... ");
  if (npurt_configure(&config, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "failed: %s\n", diagnostic.message);
    return 1;
  }
  fprintf(stderr, "ok\nwarm ... ");
  if (npurt_warm(&diagnostic) != NPURT_OK) {
    fprintf(stderr, "failed: %s\n", diagnostic.message);
    return 1;
  }
  fprintf(stderr, "ok\n");
  elf = read_file(elf_path, &elf_size);
  if (elf == NULL) {
    fprintf(stderr, "cannot read %s\n", elf_path);
    return 1;
  }

  fprintf(stderr, "phase 1: create/run twice within one session\n");
  for (cycle = 0; cycle < 2; ++cycle) {
    fprintf(stderr, " create #%d ... ", cycle);
    if (npurt_kernel_create(elf, elf_size, &kernel, &diagnostic) != NPURT_OK) {
      fprintf(stderr, "failed: %s\n", diagnostic.message);
      return 1;
    }
    fprintf(stderr, "ok\n");
    execute_once(kernel, "same session");
    fprintf(stderr, " free ... ");
    npurt_kernel_free(kernel);
    kernel = NULL;
    fprintf(stderr, "ok\n");
  }

  fprintf(stderr, "phase 2: shutdown + warm + create + run\n");
  for (cycle = 0; cycle < 2; ++cycle) {
    fprintf(stderr, " shutdown ... ");
    fflush(stderr);
    npurt_shutdown();
    fprintf(stderr, "ok\n warm ... ");
    fflush(stderr);
    if (npurt_warm(&diagnostic) != NPURT_OK) {
      fprintf(stderr, "failed: %s\n", diagnostic.message);
      return 1;
    }
    fprintf(stderr, "ok\n create ... ");
    if (npurt_kernel_create(elf, elf_size, &kernel, &diagnostic) != NPURT_OK) {
      fprintf(stderr, "failed: %s\n", diagnostic.message);
      return 1;
    }
    fprintf(stderr, "ok\n");
    execute_once(kernel, cycle == 0 ? "new session" : "new session (again)");
  }
  fprintf(stderr, "phase 3: shutdown and exit\n");
  npurt_shutdown();
  free(elf);
  fprintf(stderr, "PASS\n");
  return 0;
}
