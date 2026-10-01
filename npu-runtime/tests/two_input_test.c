/*
 * Two-input graph test: serialized and batched.
 *
 *   build/npu-runtime-two-input [--elf ELF] [--carrier BLOB]
 *
 * Defaults: kernel-lib's `add_f16.elf` patched into its two-input
 * `Maximum(1,1024)` carrier, where the correct result is `out[i] = a[i] + b[i]`.
 *
 * Checks:
 *   - the graph exposes two input tensors (and one output),
 *   - `npurt_run_inputs()` computes a + b bit-exactly (a = -4, b = 1 -> -3),
 *   - the same kernel patched a second time (clone) yields a different tile
 *     correctly,
 *   - `npurt_run_batch_inputs()` over both clones is bit-identical to running
 *     them one at a time.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npu_runtime.h"

#define ELEMENTS 1024
#define CHECK(condition, ...)                                                                      \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      printf("  [FAIL] ");                                                                         \
      printf(__VA_ARGS__);                                                                         \
      printf("\n");                                                                                \
      ++failures;                                                                                  \
    } else {                                                                                       \
      printf("  [ ok ] ");                                                                         \
      printf(__VA_ARGS__);                                                                         \
      printf("\n");                                                                                \
    }                                                                                              \
  } while (0)

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

/* fp16 bits, little endian, via the host float conversion */
static uint16_t to_half(float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  {
    uint32_t sign = (bits >> 16) & 0x8000u;
    int32_t exponent = (int32_t)((bits >> 23) & 0xffu) - 127 + 15;
    uint32_t mantissa = bits & 0x7fffffu;
    if (exponent <= 0) {
      return (uint16_t)sign;
    }
    if (exponent >= 31) {
      return (uint16_t)(sign | 0x7c00u);
    }
    {
      uint16_t half = (uint16_t)(sign | ((uint32_t)exponent << 10) | (mantissa >> 13));
      uint32_t remainder = mantissa & 0x1fffu;
      if (remainder > 0x1000u || (remainder == 0x1000u && (half & 1u) != 0)) {
        half += 1;
      }
      return half;
    }
  }
}

int main(int argc, char **argv) {
  const char *elf_path = "/home/asdf/projects/npunlock-work/kernel-lib/elf/add_f16.elf";
  const char *carrier =
      "/home/asdf/projects/npunlock-work/kernel-lib/carriers/maximum_2in_1x1024.blob";
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel *base = NULL;
  npurt_kernel *kernels[2] = {NULL, NULL};
  npurt_run_inputs_request requests[2];
  const void *inputs[2][2];
  size_t input_sizes[2][2];
  npurt_kernel_info info;
  uint8_t *elf;
  size_t elf_size = 0;
  size_t bytes = 0;
  size_t output_bytes = 0;
  uint16_t *a[2];
  uint16_t *b[2];
  uint16_t *output[2];
  uint16_t *reference[2];
  int failures = 0;
  int index;
  int element;

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--elf") == 0 && index + 1 < argc) {
      elf_path = argv[++index];
    } else if (strcmp(argv[index], "--carrier") == 0 && index + 1 < argc) {
      carrier = argv[++index];
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[index]);
      return 2;
    }
  }

  printf("npu-runtime two-input test\n  elf: %s\n  carrier: %s\n", elf_path, carrier);
  elf = read_file(elf_path, &elf_size);
  if (elf == NULL) {
    printf("  [FAIL] cannot read %s\n", elf_path);
    return 1;
  }
  npurt_config_defaults(&config);
  snprintf(config.carrier_blob, sizeof(config.carrier_blob), "%s", carrier);
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/two-input");
  config.verbosity = 0;
  npurt_diagnostic_init(&diagnostic);
  if (npurt_configure(&config, &diagnostic) != NPURT_OK || npurt_warm(&diagnostic) != NPURT_OK) {
    printf("  [FAIL] init: %s\n", diagnostic.message);
    return 1;
  }
  if (npurt_kernel_create(elf, elf_size, &base, &diagnostic) != NPURT_OK) {
    printf("  [FAIL] create: %s\n", diagnostic.message);
    return 1;
  }
  npurt_kernel_info_get(base, &info);
  CHECK(info.input_count == 2 && info.output_count == 1, "graph exposes %u inputs and %u output",
        info.input_count, info.output_count);
  if (info.input_count != 2 || info.output_count != 1) {
    return 1;
  }
  CHECK(info.tensors[0].byte_size == ELEMENTS * 2 && info.tensors[1].byte_size == ELEMENTS * 2,
        "both input tensors are %d fp16 elements", ELEMENTS);
  bytes = (size_t)info.tensors[0].byte_size;
  output_bytes = (size_t)info.tensors[2].byte_size;

  kernels[0] = base;
  if (npurt_kernel_clone(base, &kernels[1], &diagnostic) != NPURT_OK) {
    printf("  [FAIL] clone: %s\n", diagnostic.message);
    return 1;
  }
  for (index = 0; index < 2; ++index) {
    a[index] = (uint16_t *)malloc(bytes);
    b[index] = (uint16_t *)malloc(bytes);
    output[index] = (uint16_t *)calloc(1, output_bytes);
    reference[index] = (uint16_t *)calloc(1, output_bytes);
    for (element = 0; element < ELEMENTS; ++element) {
      /* tile 0: a = -4, b = 1 ; tile 1: a = 0.5, b = 0.25 */
      a[index][element] = index == 0 ? to_half(-4.0f) : to_half(0.5f);
      b[index][element] = index == 0 ? to_half(1.0f) : to_half(0.25f);
    }
    inputs[index][0] = a[index];
    inputs[index][1] = b[index];
    input_sizes[index][0] = bytes;
    input_sizes[index][1] = bytes;
    memset(&requests[index], 0, sizeof(requests[index]));
    requests[index].inputs = inputs[index];
    requests[index].input_sizes = input_sizes[index];
    requests[index].input_count = 2;
    requests[index].output = output[index];
    requests[index].output_size = output_bytes;
  }

  /* serialized reference: a + b computed per element, in fp16 */
  for (index = 0; index < 2; ++index) {
    uint16_t *expected = reference[index];
    for (element = 0; element < ELEMENTS; ++element) {
      float sum = (index == 0 ? -4.0f + 1.0f : 0.5f + 0.25f);
      expected[element] = to_half(sum);
    }
  }
  for (index = 0; index < 2; ++index) {
    if (npurt_run_inputs(kernels[index], &requests[index], NULL, &diagnostic) != NPURT_OK) {
      printf("  [FAIL] serial run %d: %s\n", index, diagnostic.message);
      return 1;
    }
  }
  CHECK(memcmp(output[0], reference[0], output_bytes) == 0, "tile 0: a + b == -3.0 exactly");
  CHECK(memcmp(output[1], reference[1], output_bytes) == 0, "tile 1: a + b == 0.75 exactly");

  /* batched: both handles in one submission, results must equal the reference */
  memset(output[0], 0, output_bytes);
  memset(output[1], 0, output_bytes);
  {
    npurt_timing timing;
    if (npurt_run_batch_inputs(kernels, requests, 2, &timing, &diagnostic) != NPURT_OK) {
      printf("  [FAIL] batch: %s\n", diagnostic.message);
      return 1;
    }
    printf("    batch of 2 two-input invocations: %.3f ms total (%.3f ms/call)\n",
           (double)timing.total_ns / 1e6, (double)timing.total_ns / 2e6);
  }
  CHECK(memcmp(output[0], reference[0], output_bytes) == 0 &&
            memcmp(output[1], reference[1], output_bytes) == 0,
        "batched results are bit-identical to the serialized ones");

  npurt_kernel_free(kernels[1]);
  npurt_shutdown();
  for (index = 0; index < 2; ++index) {
    free(a[index]);
    free(b[index]);
    free(output[index]);
    free(reference[index]);
  }
  free(elf);
  printf("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures,
         failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
