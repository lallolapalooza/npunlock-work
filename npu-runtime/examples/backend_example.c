/*
 * npu-runtime usage example — the shape a ggml/llama.cpp backend wants.
 *
 * Build:  make build/npu-runtime-example
 * Run:    ./build/npu-runtime-example            (compiles the kernel at start-up)
 *         ./build/npu-runtime-example --elf k.elf (uses a prebuilt kernel ELF)
 *
 * The example walks the three lifetimes a backend actually has:
 *
 *   1. PROCESS   npurt_warm()          once, at backend init (≈5 ms)
 *   2. KERNEL    npurt_kernel_create() once per shape/op pair; it patches the
 *                                      carrier, compiles the graph, initializes
 *                                      it and caches the session (≈6-19 ms)
 *   3. INVOCATION npurt_run()          per graph_compute, from one thread
 *                                      (≈1 ms for the 1x16 carrier)
 *
 * Nothing here spawns a process, rebuilds a bundle or re-creates the graph;
 * that is the whole point of the runtime.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "npu_runtime.h"

/* The carrier graph is abs-add-1x16-tile1: 16 fp16 elements in and out, its
 * DPU step adds a fixed ramp, and the patched ACT sees fp16(x + ramp). */
#define TILE_ELEMENTS 16

static const float g_input[TILE_ELEMENTS] = {-4.0f, -2.0f, -1.0f, -0.5f, 0.0f,  0.25f, 0.5f, 1.0f,
                                             2.0f,  3.0f,  4.0f,  8.0f,  16.0f, 32.0f, 64.0f, 128.0f};

/* Same kernel as npunlock's tests/fixtures/npu3720/add1-fp16.c: out[i] = in[i] + 1. */
static const char *g_kernel_source =
    "#include <npunlock/npu3720_kernel.h>\n"
    "static __attribute__((always_inline)) inline unsigned ld32(const unsigned char *p) {\n"
    "  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);\n"
    "}\n"
    "void controlled_act(unsigned layerParams) {\n"
    "  const unsigned char *params = (const unsigned char *)layerParams;\n"
    "  unsigned rank = ld32(params + 0xc);\n"
    "  const unsigned char *dims = (const unsigned char *)ld32(params + 0x10);\n"
    "  if (rank == 0 || rank > 15 || !dims) return;\n"
    "  unsigned count = 1;\n"
    "  for (unsigned d = 0; d < rank; ++d) {\n"
    "    unsigned dim = ld32(dims + d * 4);\n"
    "    if (dim == 0 || dim > 16 / count) return;\n"
    "    count *= dim;\n"
    "  }\n"
    "  const __fp16 *in = (const __fp16 *)ld32(params);\n"
    "  __fp16 *out = (__fp16 *)ld32(params + 0x28);\n"
    "  for (unsigned i = 0; i < count; ++i) out[i] = in[i] + 1.0f;\n"
    "}\n";

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

static float to_float(uint16_t half) {
  uint32_t sign = (uint32_t)(half & 0x8000u) << 16;
  uint32_t exponent = (half >> 10) & 0x1fu;
  uint32_t mantissa = half & 0x3ffu;
  uint32_t bits;
  float value;
  if (exponent == 0) {
    bits = sign | (mantissa << 13);
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

static uint64_t now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

int main(int argc, char **argv) {
  const char *elf_path = NULL;
  const char *carrier = NULL;
  npurt_config config;
  npurt_diagnostic diagnostic;
  npurt_kernel *kernel = NULL;
  npurt_kernel_info info;
  uint8_t *elf = NULL;
  size_t elf_size = 0;
  uint16_t input[TILE_ELEMENTS];
  uint16_t expected[TILE_ELEMENTS];
  uint16_t output[TILE_ELEMENTS];
  uint64_t start;
  int index;

  for (index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--elf") == 0 && index + 1 < argc) {
      elf_path = argv[++index];
    } else if (strcmp(argv[index], "--carrier") == 0 && index + 1 < argc) {
      carrier = argv[++index];
    }
  }

  /* ---------------- 1. process lifetime: bring up the NPU once ------------- */
  npurt_config_defaults(&config); /* defaults already point at the npunlock tree */
  if (carrier != NULL) {
    snprintf(config.carrier_blob, sizeof(config.carrier_blob), "%s", carrier);
  }
  snprintf(config.work_dir, sizeof(config.work_dir), "/tmp/NPURuntime/example");
  config.verbosity = 1;
  npurt_diagnostic_init(&diagnostic);
  if (npurt_configure(&config, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "configure: %s\n", diagnostic.message);
    return 1;
  }
  start = now_ns();
  if (npurt_warm(&diagnostic) != NPURT_OK) {
    fprintf(stderr, "warm: %s\n", diagnostic.message);
    return 1;
  }
  printf("warm: %.3f ms\n", (double)(now_ns() - start) / 1e6);

  /* ---------------- 2. kernel lifetime: compile (optional) and load -------- */
  if (elf_path == NULL) {
    start = now_ns();
    if (npurt_compile_source(g_kernel_source, strlen(g_kernel_source), &elf, &elf_size,
                             &diagnostic) != NPURT_OK) {
      fprintf(stderr, "compile: %s\n", diagnostic.message);
      return 1;
    }
    printf("compile (moviCompile -> moviAsm -> moviLLD): %.3f ms for %zu bytes\n",
           (double)(now_ns() - start) / 1e6, elf_size);
  } else {
    elf = read_file(elf_path, &elf_size);
    if (elf == NULL) {
      fprintf(stderr, "cannot read %s\n", elf_path);
      return 1;
    }
    printf("prebuilt kernel: %s (%zu bytes)\n", elf_path, elf_size);
  }
  start = now_ns();
  if (npurt_kernel_create(elf, elf_size, &kernel, &diagnostic) != NPURT_OK) {
    fprintf(stderr, "kernel create: %s\n", diagnostic.message);
    return 1;
  }
  npurt_kernel_info_get(kernel, &info);
  printf("kernel session: %.3f ms (patch %.3f + graph create %.3f + arg query %.3f + init %.3f)\n",
         (double)(now_ns() - start) / 1e6, (double)info.patch_ns / 1e6,
         (double)info.graph_create_ns / 1e6, (double)info.query_ns / 1e6,
         (double)info.init_ns / 1e6);

  /* A second create with the same ELF is a pure cache hit: no device work. */
  {
    npurt_kernel *cached = NULL;
    start = now_ns();
    if (npurt_kernel_create(elf, elf_size, &cached, &diagnostic) != NPURT_OK ||
        cached != kernel) {
      fprintf(stderr, "kernel cache miss on the second create\n");
      return 1;
    }
    printf("second create (cache hit): %.3f ms\n", (double)(now_ns() - start) / 1e6);
  }

  /* ---------------- 3. invocation lifetime: one call per graph_compute ----- */
  for (index = 0; index < TILE_ELEMENTS; ++index) {
    /* the carrier's ramp is fp16(i/15 - 0.5) */
    float ramp = to_float(to_half((float)index / 15.0f - 0.5f));
    input[index] = to_half(g_input[index]);
    expected[index] = to_half(to_float(to_half(to_float(input[index]) + ramp)) + 1.0f);
  }
  {
    int call;
    double total = 0.0;
    for (call = 0; call < 5; ++call) {
      npurt_run_request request;
      npurt_timing timing;
      memset(&request, 0, sizeof(request));
      request.input = input;
      request.input_size = sizeof(input);
      request.output = output;
      request.output_size = sizeof(output);
      memset(output, 0, sizeof(output));
      if (npurt_run(kernel, &request, &timing, &diagnostic) != NPURT_OK) {
        fprintf(stderr, "run %d: %s\n", call, diagnostic.message);
        return 1;
      }
      if (call > 0) {
        total += (double)timing.total_ns;
      }
      printf("run %d: %.3f ms (device %.3f ms)\n", call, (double)timing.total_ns / 1e6,
             (double)timing.wait_ns / 1e6);
    }
    printf("steady state: %.3f ms/call, %.0f calls/s\n", total / 4.0 / 1e6,
           1e9 / (total / 4.0));
  }
  {
    int match = memcmp(output, expected, sizeof(expected)) == 0;
    printf("output vs host oracle: %s\n", match ? "exact" : "MISMATCH");
    if (!match) {
      for (index = 0; index < TILE_ELEMENTS; ++index) {
        printf("  [%2d] got %04x expected %04x\n", index, output[index], expected[index]);
      }
    }
    /* in a backend this is where the f16 tile is copied back into the ggml
     * tensor; the runtime already wrote it into `output`. */
    npurt_shutdown();
    free(elf);
    return match ? 0 : 1;
  }
}
