/* Diagnostics, clocks, SHA-256 and configuration handling. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "npurt_internal.h"

#ifndef NPURT_DEFAULT_NPUNLOCK_ROOT
#define NPURT_DEFAULT_NPUNLOCK_ROOT "/home/asdf/projects/npunlock-work/npunlock"
#endif

const char *npurt_status_name(npurt_status status) {
  switch (status) {
  case NPURT_OK:
    return "ok";
  case NPURT_ERR_ARGUMENT:
    return "argument";
  case NPURT_ERR_STATE:
    return "state";
  case NPURT_ERR_IO:
    return "io";
  case NPURT_ERR_COMPILE:
    return "compile";
  case NPURT_ERR_PATCH:
    return "patch";
  case NPURT_ERR_DEVICE:
    return "device";
  case NPURT_ERR_NO_NPU:
    return "no-npu";
  case NPURT_ERR_TIMEOUT:
    return "timeout";
  case NPURT_ERR_SHAPE:
    return "shape";
  case NPURT_ERR_NOT_FOUND:
    return "not-found";
  case NPURT_ERR_UNSUPPORTED:
    return "unsupported";
  case NPURT_ERR_OUT_OF_MEMORY:
    return "out-of-memory";
  default:
    return "unknown";
  }
}

void npurt_diagnostic_init(npurt_diagnostic *diagnostic) {
  if (diagnostic != NULL) {
    diagnostic->message[0] = '\0';
  }
}

void npurt_set_diagnostic(npurt_diagnostic *diagnostic, const char *format, ...) {
  va_list arguments;
  if (diagnostic == NULL) {
    return;
  }
  va_start(arguments, format);
  vsnprintf(diagnostic->message, sizeof(diagnostic->message), format, arguments);
  va_end(arguments);
}

uint64_t npurt_now_ns(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

int npurt_verbosity(void) {
  return npurt_state_get()->config.verbosity;
}

/* --- SHA-256 (self-contained; avoids depending on private npunlock symbols) - */

typedef struct npurt_sha256_context {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t block[64];
  size_t block_size;
} npurt_sha256_context;

static const uint32_t npurt_sha256_k[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

static uint32_t npurt_rotr(uint32_t value, unsigned count) {
  return (value >> count) | (value << (32u - count));
}

static void npurt_sha256_transform(npurt_sha256_context *context, const uint8_t block[64]) {
  uint32_t w[64];
  uint32_t a, b, c, d, e, f, g, h;
  unsigned index;
  for (index = 0; index < 16; ++index) {
    w[index] = ((uint32_t)block[index * 4] << 24) | ((uint32_t)block[index * 4 + 1] << 16) |
               ((uint32_t)block[index * 4 + 2] << 8) | (uint32_t)block[index * 4 + 3];
  }
  for (index = 16; index < 64; ++index) {
    uint32_t s0 = npurt_rotr(w[index - 15], 7) ^ npurt_rotr(w[index - 15], 18) ^ (w[index - 15] >> 3);
    uint32_t s1 = npurt_rotr(w[index - 2], 17) ^ npurt_rotr(w[index - 2], 19) ^ (w[index - 2] >> 10);
    w[index] = w[index - 16] + s0 + w[index - 7] + s1;
  }
  a = context->state[0];
  b = context->state[1];
  c = context->state[2];
  d = context->state[3];
  e = context->state[4];
  f = context->state[5];
  g = context->state[6];
  h = context->state[7];
  for (index = 0; index < 64; ++index) {
    uint32_t s1 = npurt_rotr(e, 6) ^ npurt_rotr(e, 11) ^ npurt_rotr(e, 25);
    uint32_t ch = (e & f) ^ ((~e) & g);
    uint32_t temp1 = h + s1 + ch + npurt_sha256_k[index] + w[index];
    uint32_t s0 = npurt_rotr(a, 2) ^ npurt_rotr(a, 13) ^ npurt_rotr(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temp2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  context->state[0] += a;
  context->state[1] += b;
  context->state[2] += c;
  context->state[3] += d;
  context->state[4] += e;
  context->state[5] += f;
  context->state[6] += g;
  context->state[7] += h;
}

static void npurt_sha256_init(npurt_sha256_context *context) {
  context->state[0] = 0x6a09e667u;
  context->state[1] = 0xbb67ae85u;
  context->state[2] = 0x3c6ef372u;
  context->state[3] = 0xa54ff53au;
  context->state[4] = 0x510e527fu;
  context->state[5] = 0x9b05688cu;
  context->state[6] = 0x1f83d9abu;
  context->state[7] = 0x5be0cd19u;
  context->bit_count = 0;
  context->block_size = 0;
  memset(context->block, 0, sizeof(context->block));
}

static void npurt_sha256_update(npurt_sha256_context *context, const uint8_t *data, size_t size) {
  size_t index = 0;
  context->bit_count += (uint64_t)size * 8u;
  if (context->block_size != 0) {
    size_t need = 64 - context->block_size;
    size_t take = size < need ? size : need;
    memcpy(context->block + context->block_size, data, take);
    context->block_size += take;
    index += take;
    if (context->block_size == 64) {
      npurt_sha256_transform(context, context->block);
      context->block_size = 0;
    }
  }
  for (; index + 64 <= size; index += 64) {
    npurt_sha256_transform(context, data + index);
  }
  if (index < size) {
    memcpy(context->block, data + index, size - index);
    context->block_size = size - index;
  }
}

static void npurt_sha256_final(npurt_sha256_context *context, uint8_t digest[32]) {
  uint64_t bits = context->bit_count;
  unsigned index;
  context->block[context->block_size++] = 0x80u;
  if (context->block_size > 56) {
    memset(context->block + context->block_size, 0, 64 - context->block_size);
    npurt_sha256_transform(context, context->block);
    context->block_size = 0;
  }
  memset(context->block + context->block_size, 0, 56 - context->block_size);
  for (index = 0; index < 8; ++index) {
    context->block[56 + index] = (uint8_t)(bits >> (56u - index * 8u));
  }
  npurt_sha256_transform(context, context->block);
  for (index = 0; index < 8; ++index) {
    digest[index * 4] = (uint8_t)(context->state[index] >> 24);
    digest[index * 4 + 1] = (uint8_t)(context->state[index] >> 16);
    digest[index * 4 + 2] = (uint8_t)(context->state[index] >> 8);
    digest[index * 4 + 3] = (uint8_t)context->state[index];
  }
}

void npurt_sha256_hex(const uint8_t *data, size_t size, char hex[NPURT_SHA256_HEX_BYTES]) {
  static const char *digits = "0123456789abcdef";
  npurt_sha256_context context;
  uint8_t digest[32];
  unsigned index;
  npurt_sha256_init(&context);
  npurt_sha256_update(&context, data, size);
  npurt_sha256_final(&context, digest);
  for (index = 0; index < 32; ++index) {
    hex[index * 2] = digits[digest[index] >> 4];
    hex[index * 2 + 1] = digits[digest[index] & 0xfu];
  }
  hex[64] = '\0';
}

void *npurt_malloc(size_t size) { return malloc(size == 0 ? 1 : size); }

void npurt_free(void *pointer) { free(pointer); }

/* --- configuration -------------------------------------------------------- */

static void npurt_copy_string(char *destination, size_t capacity, const char *source) {
  if (capacity == 0) {
    return;
  }
  if (source == NULL) {
    destination[0] = '\0';
    return;
  }
  snprintf(destination, capacity, "%s", source);
}

static const char *npurt_env(const char *name) {
  const char *value = getenv(name);
  return (value != NULL && value[0] != '\0') ? value : NULL;
}

static void npurt_join_path(char *destination, size_t capacity, const char *left,
                           const char *right) {
  if (left == NULL || left[0] == '\0') {
    npurt_copy_string(destination, capacity, right);
    return;
  }
  if (right == NULL || right[0] == '\0') {
    npurt_copy_string(destination, capacity, left);
    return;
  }
  {
    size_t length = strlen(left);
    if (left[length - 1] == '/') {
      snprintf(destination, capacity, "%s%s", left, right);
    } else {
      snprintf(destination, capacity, "%s/%s", left, right);
    }
  }
}

void npurt_config_defaults(npurt_config *config) {
  const char *root;
  const char *value;
  char scratch[NPURT_PATH_BYTES];
  char scratch2[NPURT_PATH_BYTES];
  if (config == NULL) {
    return;
  }
  memset(config, 0, sizeof(*config));
  root = npurt_env("NPURT_NPUNLOCK_ROOT");
  if (root == NULL) {
    root = NPURT_DEFAULT_NPUNLOCK_ROOT;
  }
  value = npurt_env("NPURT_TOOLCHAIN_DIR");
  if (value != NULL) {
    npurt_copy_string(config->toolchain_dir, sizeof(config->toolchain_dir), value);
  } else {
    npurt_join_path(scratch, sizeof(scratch), root, "../shave-toolchain-root");
    npurt_copy_string(config->toolchain_dir, sizeof(config->toolchain_dir), scratch);
  }
  npurt_join_path(scratch, sizeof(scratch), root, "tests/fixtures/npu3720/abs-add-1x16-tile1.blob");
  npurt_copy_string(config->carrier_blob, sizeof(config->carrier_blob),
                    npurt_env("NPURT_CARRIER_BLOB") != NULL ? npurt_env("NPURT_CARRIER_BLOB")
                                                            : scratch);
  npurt_join_path(scratch, sizeof(scratch), root, "src/shavecc/shave_kernel.ld");
  npurt_copy_string(config->link_script, sizeof(config->link_script), scratch);
  npurt_join_path(scratch, sizeof(scratch), root, "../corpus/include");
  npurt_copy_string(config->include_dir, sizeof(config->include_dir),
                    npurt_env("NPURT_INCLUDE_DIR") != NULL ? npurt_env("NPURT_INCLUDE_DIR")
                                                           : scratch);
  if (npurt_env("NPURT_WORK_DIR") != NULL) {
    npurt_copy_string(scratch2, sizeof(scratch2), npurt_env("NPURT_WORK_DIR"));
  } else if (getenv("TMPDIR") != NULL && getenv("TMPDIR")[0] != '\0') {
    npurt_copy_string(scratch2, sizeof(scratch2), getenv("TMPDIR"));
  } else {
    npurt_copy_string(scratch2, sizeof(scratch2), "/tmp");
  }
  npurt_join_path(scratch, sizeof(scratch), scratch2, "npu-runtime");
  npurt_copy_string(config->work_dir, sizeof(config->work_dir), scratch);
  npurt_copy_string(config->entry_symbol, sizeof(config->entry_symbol),
                    npurt_env("NPURT_ENTRY_SYMBOL") != NULL ? npurt_env("NPURT_ENTRY_SYMBOL")
                                                            : "controlled_act");
  npurt_copy_string(config->extra_cflags, sizeof(config->extra_cflags),
                    npurt_env("NPURT_EXTRA_CFLAGS") != NULL ? npurt_env("NPURT_EXTRA_CFLAGS") : "");
  config->link_library_count = 0;
  config->device_index = UINT32_MAX;
  config->max_cached_kernels = 8;
  config->default_timeout_ns = 5000000000ull;
  config->compile_timeout_ns = 25000000000ull;
  config->verbosity = 1;
  config->rebind_arguments_each_run = 1;
  value = npurt_env("NPURT_VERBOSITY");
  if (value != NULL) {
    config->verbosity = atoi(value);
  }
}

