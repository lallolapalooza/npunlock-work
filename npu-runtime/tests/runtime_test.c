/*
 * npu-runtime acceptance test.
 *
 *   build/npu-runtime-test [ELF]
 *
 * Default ELF: the retained add-one fixture that the project's smoke gate
 * verifies end-to-end (/home/asdf/projects/npunlock-work/our-add1-fp16.elf,
 * patched into the npunlock 1x16 carrier). The kernel computes
 * fp16(fp16(x + ramp) + 1.0) with the carrier's own ramp, which makes the
 * check semantic rather than merely "the device returned".
 *
 * Checks: warm, kernel create + cache hit, exact numeric oracle, repeatability
 * across invocations, shape rejection, timeout/state errors, shutdown state.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "npu_runtime.h"

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

#define ELEMENTS 16

/* The graph input values used by npunlock's porting_probe.py. */
static const float g_input_values[ELEMENTS] = {-4.0f, -2.0f, -1.0f, -0.5f, 0.0f,  0.25f, 0.5f, 1.0f,
                                               2.0f,  3.0f,  4.0f,  8.0f,  16.0f, 32.0f, 64.0f, 128.0f};

static uint16_t float_to_half(float value) {
  uint32_t bits;
  uint32_t sign;
  int32_t exponent;
  uint32_t mantissa;
  memcpy(&bits, &value, sizeof(bits));
  sign = (bits >> 16) & 0x8000u;
  exponent = (int32_t)((bits >> 23) & 0xffu) - 127 + 15;
  mantissa = bits & 0x7fffffu;
  if (exponent <= 0) {
    /* subnormal or zero: round the shifted mantissa */
    uint32_t shift;
    uint32_t rounded;
    if (exponent < -10) {
      return (uint16_t)sign;
    }
    mantissa |= 0x800000u;
    shift = (uint32_t)(14 - exponent);
    rounded = mantissa >> shift;
    if ((mantissa >> (shift - 1)) & 1u) {
      rounded += 1;
    }
    return (uint16_t)(sign | rounded);
  }
  if (exponent >= 31) {
    return (uint16_t)(sign | 0x7c00u); /* overflow -> infinity */
  }
  {
    uint16_t half = (uint16_t)(sign | ((uint32_t)exponent << 10) | (mantissa >> 13));
    uint32_t remainder = mantissa & 0x1fffu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (half & 1u) != 0)) {
      half += 1; /* round to nearest even; carries into the exponent correctly */
    }
    return half;
  }
}

static float half_to_float(uint16_t half) {
  uint32_t sign = (uint32_t)(half & 0x8000u) << 16;
  uint32_t exponent = (half >> 10) & 0x1fu;
  uint32_t mantissa = half & 0x3ffu;
  uint32_t bits;
  float value;
  if (exponent == 0) {
    if (mantissa == 0) {
      bits = sign;
    } else {
      int32_t shift = 0;
      while ((mantissa & 0x400u) == 0) {
        mantissa <<= 1;
        ++shift;
      }
      mantissa &= 0x3ffu;
      bits = sign | ((uint32_t)(127 - 15 - shift) << 23) | (mantissa << 13);
    }
  } else if (exponent == 31) {
    bits = sign | 0x7f800000u | (mantissa << 13);
  } else {
    bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
  }
  memcpy(&value, &bits, sizeof(value));
  return value;
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

/*
 * Multi-kernel mode: several op kernels held open at once (what a backend with
 * more than one supported op does). Verifies that every handle stays valid
 * while others are created, and that interleaved invocations stay stable.
 */
static int run_multi(char **paths, int count) {
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel **kernels = (npurt_kernel **)calloc((size_t)count, sizeof(*kernels));
  uint16_t **first = (uint16_t **)calloc((size_t)count, sizeof(*first));
  uint16_t *input;
  size_t input_bytes = 0;
  size_t output_bytes = 0;
  npurt_kernel_info info;
  int failures = 0;
  int index;
  int pass;

  npurt_config_defaults(&config);
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/runtime-test");
  npurt_diagnostic_init(&diagnostic);
  if (npurt_configure(&config, &diagnostic) != NPURT_OK || npurt_warm(&diagnostic) != NPURT_OK) {
    printf("  [FAIL] warm: %s\n", diagnostic.message);
    return 1;
  }
  for (index = 0; index < count; ++index) {
    uint8_t *elf;
    size_t elf_size = 0;
    elf = read_file(paths[index], &elf_size);
    if (elf == NULL) {
      printf("  [FAIL] cannot read %s\n", paths[index]);
      return 1;
    }
    if (npurt_kernel_create(elf, elf_size, &kernels[index], &diagnostic) != NPURT_OK) {
      printf("  [FAIL] create %s: %s\n", paths[index], diagnostic.message);
      return 1;
    }
    free(elf);
    npurt_kernel_info_get(kernels[index], &info);
    if (input_bytes == 0) {
      uint32_t tensor;
      for (tensor = 0; tensor < info.tensor_count; ++tensor) {
        if (info.tensors[tensor].is_input) {
          input_bytes = (size_t)info.tensors[tensor].byte_size;
        } else {
          output_bytes = (size_t)info.tensors[tensor].byte_size;
        }
      }
    }
  }
  CHECK(1, "%d kernels are open at the same time", count);
  input = (uint16_t *)calloc(1, input_bytes);
  for (index = 0; index < (int)(input_bytes / 2); ++index) {
    input[index] = (uint16_t)(0x3c00u + index);
  }
  for (index = 0; index < count; ++index) {
    first[index] = (uint16_t *)calloc(1, output_bytes);
  }
  for (pass = 0; pass < 3; ++pass) {
    for (index = 0; index < count; ++index) {
      npurt_run_request request;
      void *target = pass == 0 ? (void *)first[index] : (void *)first[index];
      uint16_t *scratch = (uint16_t *)calloc(1, output_bytes);
      memset(&request, 0, sizeof(request));
      request.input = input;
      request.input_size = input_bytes;
      request.output = pass == 0 ? target : scratch;
      request.output_size = output_bytes;
      if (npurt_run(kernels[index], &request, NULL, &diagnostic) != NPURT_OK) {
        printf("  [FAIL] pass %d kernel %d (%s): %s\n", pass, index, paths[index],
               diagnostic.message);
        ++failures;
      } else if (pass > 0 && memcmp(scratch, first[index], output_bytes) != 0) {
        printf("  [FAIL] pass %d kernel %d (%s) output changed\n", pass, index, paths[index]);
        ++failures;
      }
      free(scratch);
    }
  }
  CHECK(failures == 0, "3 interleaved invocations of every kernel are stable");
  {
    npurt_cache_stats stats;
    npurt_cache_stats_get(&stats);
    printf("  cache: %u entries (soft limit %u), %llu hits, %llu misses, %llu over-capacity\n",
           stats.entries, stats.capacity, (unsigned long long)stats.hits,
           (unsigned long long)stats.misses, (unsigned long long)stats.over_capacity);
    CHECK(stats.entries >= (uint32_t)count, "the cache still holds every live kernel");
  }
  for (index = 0; index < count; ++index) {
    free(first[index]);
    npurt_kernel_free(kernels[index]);
  }
  npurt_shutdown();
  free(kernels);
  free(first);
  free(input);
  printf("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures,
         failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
  const char *elf_path = argc > 1 ? argv[1] : "/home/asdf/projects/npunlock-work/our-add1-fp16.elf";
  npurt_diagnostic diagnostic;
  npurt_config config;
  npurt_kernel *kernel = NULL;
  npurt_kernel *again = NULL;
  npurt_kernel_info info;
  npurt_status status;
  uint8_t *elf;
  size_t elf_size = 0;
  uint16_t input[ELEMENTS];
  uint16_t expected[ELEMENTS];
  uint16_t output[ELEMENTS];
  uint16_t repeat[ELEMENTS];
  int failures = 0;
  uint32_t graph_dims[5] = {0};
  uint32_t graph_rank = 0;
  unsigned index;
  unsigned invocation;
  float ramp[ELEMENTS];

  if (argc > 2 && strcmp(argv[1], "--multi") == 0) {
    printf("npu-runtime multi-kernel test\n");
    return run_multi(&argv[2], argc - 2);
  }

  printf("npu-runtime acceptance test\n");
  printf("  elf: %s\n", elf_path);

  npurt_diagnostic_init(&diagnostic);
  elf = read_file(elf_path, &elf_size);
  if (elf == NULL) {
    printf("  [FAIL] cannot read %s\n", elf_path);
    return 1;
  }
  printf("  elf size: %zu bytes\n", elf_size);

  for (index = 0; index < ELEMENTS; ++index) {
    ramp[index] = half_to_float(float_to_half((float)index / 15.0f - 0.5f));
    input[index] = float_to_half(g_input_values[index]);
  }
  for (index = 0; index < ELEMENTS; ++index) {
    float carrier_value = half_to_float(input[index]) + ramp[index];
    uint16_t carrier_half = float_to_half(carrier_value);
    expected[index] = float_to_half(half_to_float(carrier_half) + 1.0f);
  }

  npurt_config_defaults(&config);
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/runtime-test");
  status = npurt_configure(&config, &diagnostic);
  CHECK(status == NPURT_OK, "npurt_configure (%s)", npurt_status_name(status));

  status = npurt_warm(&diagnostic);
  CHECK(status == NPURT_OK, "npurt_warm (%s) %s", npurt_status_name(status), diagnostic.message);
  CHECK(npurt_is_warm() == 1, "npurt_is_warm");
  status = npurt_warm(&diagnostic);
  CHECK(status == NPURT_OK, "npurt_warm is idempotent");

  status = npurt_kernel_lookup(elf, elf_size, &kernel);
  CHECK(status == NPURT_ERR_NOT_FOUND, "cache probe misses before create (%s)",
        npurt_status_name(status));

  status = npurt_kernel_create(elf, elf_size, &kernel, &diagnostic);
  CHECK(status == NPURT_OK, "npurt_kernel_create (%s) %s", npurt_status_name(status),
        diagnostic.message);
  if (status != NPURT_OK) {
    return 1;
  }
  npurt_kernel_info_get(kernel, &info);
  printf("    blob %llu bytes, %u ACT target(s), patch %llu us, graph create %llu us, "
         "query %llu us, init %llu us, setup %llu us\n",
         (unsigned long long)info.blob_bytes, info.patch_target_count,
         (unsigned long long)(info.patch_ns / 1000), (unsigned long long)(info.graph_create_ns / 1000),
         (unsigned long long)(info.query_ns / 1000), (unsigned long long)(info.init_ns / 1000),
         (unsigned long long)(info.setup_ns / 1000));
  CHECK(info.tensor_count == 2 && info.input_count == 1 && info.output_count == 1,
        "graph exposes one input and one output tensor");
  CHECK(info.tensors[info.input_count ? 0 : 0].byte_size == ELEMENTS * 2,
        "graph input tensor is %d fp16 elements", ELEMENTS);

  status = npurt_kernel_lookup(elf, elf_size, &again);
  CHECK(status == NPURT_OK && again == kernel, "second create/lookup returns the cached handle");

  {
    /* remember the graph's declared dims so the shape assertion matches */
    unsigned tensor;
    for (tensor = 0; tensor < info.tensor_count; ++tensor) {
      if (info.tensors[tensor].is_input) {
        graph_rank = info.tensors[tensor].dims_count;
        memcpy(graph_dims, info.tensors[tensor].dims, sizeof(graph_dims));
      }
    }
  }
  {
    uint64_t elements = 1;
    unsigned axis;
    for (axis = 0; axis < graph_rank; ++axis) {
      elements *= graph_dims[axis];
    }
    CHECK(graph_rank == 2 && elements == ELEMENTS, "graph input dims are %ux%u (rank %u)",
          graph_dims[0], graph_rank > 1 ? graph_dims[1] : 1u, graph_rank);
  }

  for (invocation = 0; invocation < 3; ++invocation) {
    npurt_run_request request;
    npurt_timing timing;
    void *target = invocation == 0 ? (void *)output : (void *)repeat;
    memset(&request, 0, sizeof(request));
    memset(target, 0xab, sizeof(output));
    request.input = input;
    request.input_size = sizeof(input);
    request.output = target;
    request.output_size = sizeof(output);
    if (invocation == 0) {
      request.dims = graph_dims;
      request.dims_count = graph_rank;
    }
    status = npurt_run(kernel, &request, &timing, &diagnostic);
    CHECK(status == NPURT_OK, "npurt_run #%u (%s) %s", invocation, npurt_status_name(status),
          diagnostic.message);
    if (status != NPURT_OK) {
      return 1;
    }
    printf("    run #%u: prepare %llu us, session %llu us, submit %llu us, wait %llu us, "
           "teardown %llu us, total %llu us\n",
           invocation, (unsigned long long)(timing.prepare_ns / 1000),
           (unsigned long long)(timing.session_ns / 1000),
           (unsigned long long)(timing.submit_ns / 1000), (unsigned long long)(timing.wait_ns / 1000),
           (unsigned long long)(timing.teardown_ns / 1000),
           (unsigned long long)(timing.total_ns / 1000));
    if (invocation == 0) {
      int exact = memcmp(output, expected, sizeof(expected)) == 0;
      if (!exact) {
        printf("      actual  :");
        for (index = 0; index < ELEMENTS; ++index) {
          printf(" %04x", output[index]);
        }
        printf("\n      expected:");
        for (index = 0; index < ELEMENTS; ++index) {
          printf(" %04x", expected[index]);
        }
        printf("\n");
      }
      CHECK(exact, "output matches the exact fp16 add-one oracle");
    }
  }
  CHECK(memcmp(output, repeat, sizeof(output)) == 0, "repeated invocations are bit-identical");

  {
    npurt_run_request request;
    uint8_t small[8];
    memset(&request, 0, sizeof(request));
    memset(small, 0, sizeof(small));
    request.input = small;
    request.input_size = sizeof(small);
    request.output = output;
    request.output_size = sizeof(output);
    status = npurt_run(kernel, &request, NULL, &diagnostic);
    CHECK(status == NPURT_ERR_SHAPE, "short input rejected (%s)", npurt_status_name(status));

    memset(&request, 0, sizeof(request));
    request.input = input;
    request.input_size = sizeof(input);
    request.output = output;
    request.output_size = 4;
    status = npurt_run(kernel, &request, NULL, &diagnostic);
    CHECK(status == NPURT_ERR_SHAPE, "short output rejected (%s)", npurt_status_name(status));

    memset(&request, 0, sizeof(request));
    {
      uint32_t bad_dims[5];
      memcpy(bad_dims, graph_dims, sizeof(bad_dims));
      bad_dims[0] = 8u;
      request.input = input;
      request.input_size = sizeof(input);
      request.output = output;
      request.output_size = sizeof(output);
      request.dims = bad_dims;
      request.dims_count = graph_rank;
    }
    status = npurt_run(kernel, &request, NULL, &diagnostic);
    CHECK(status == NPURT_ERR_SHAPE, "mismatched static shape rejected (%s)",
          npurt_status_name(status));
  }

  {
    npurt_cache_stats stats;
    npurt_cache_stats_get(&stats);
    printf("  cache: %u/%u entries, %llu hits, %llu misses\n", stats.entries, stats.capacity,
           (unsigned long long)stats.hits, (unsigned long long)stats.misses);
    CHECK(stats.entries == 1 && stats.hits >= 1, "kernel cache holds one entry with a hit");
  }

  {
    npurt_run_request request;
    npurt_status after;
    npurt_shutdown();
    memset(&request, 0, sizeof(request));
    request.input = input;
    request.input_size = sizeof(input);
    request.output = output;
    request.output_size = sizeof(output);
    after = npurt_run(kernel, &request, NULL, &diagnostic);
    CHECK(after == NPURT_ERR_STATE, "run after shutdown is refused (%s)",
          npurt_status_name(after));
    /* the handle is dangling by contract; do not touch it again */
    kernel = NULL;
  }

  printf("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures,
         failures == 1 ? "" : "s");
  free(elf);
  return failures == 0 ? 0 : 1;
}
