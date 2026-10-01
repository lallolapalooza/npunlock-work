/* Multi-entry ACT kernel family: every entry is the proven add-one FP16 path
   with a different number of +1.0f accumulations, so all entries use the
   silicon-verified LD.32.F16.F32 / SAU.ADD.F32 / ST.16.F32.F16 sequence. */
#include <npunlock/npu3720_kernel.h>

static __attribute__((always_inline)) inline unsigned multi_ld32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

#define MULTI_PROLOGUE                                                                   \
  const unsigned char *params = (const unsigned char *)layerParams;                      \
  unsigned rank = multi_ld32(params + 0xcu);                                             \
  const unsigned char *dims = (const unsigned char *)multi_ld32(params + 0x10u);         \
  if (rank == 0u || rank > 15u || dims == 0) return;                                     \
  unsigned count = 1u;                                                                   \
  for (unsigned d = 0u; d < rank; ++d) {                                                 \
    unsigned dim = multi_ld32(dims + d * 4u);                                            \
    if (dim == 0u || dim > 16u / count) return;                                          \
    count *= dim;                                                                        \
  }                                                                                      \
  const __fp16 *in = (const __fp16 *)multi_ld32(params);                                 \
  __fp16 *out = (__fp16 *)multi_ld32(params + 0x28u);                                    \
  for (unsigned i = 0u; i < count; ++i)

void controlled_act(unsigned layerParams) { MULTI_PROLOGUE out[i] = in[i] + 1.0f; }
void multi_b(unsigned layerParams) { MULTI_PROLOGUE out[i] = in[i] + 1.0f + 1.0f; }
void multi_c(unsigned layerParams) { MULTI_PROLOGUE out[i] = in[i] + 1.0f + 1.0f + 1.0f; }
void multi_d(unsigned layerParams) { MULTI_PROLOGUE out[i] = in[i] + 1.0f + 1.0f + 1.0f + 1.0f; }
