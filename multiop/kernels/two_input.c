/* ACT kernel for a two-input carrier: reads input record 0, ignores record 1,
   writes output record 2 with in0 + 2.0. */
#include <npunlock/npu3720_kernel.h>
static __attribute__((always_inline)) inline unsigned m32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned char *params = (const unsigned char *)layerParams;
  unsigned rank = m32(params + 0xcu);
  const unsigned char *dims = (const unsigned char *)m32(params + 0x10u);
  if (rank == 0u || rank > 15u || dims == 0) return;
  unsigned count = 1u;
  for (unsigned d = 0u; d < rank; ++d) { unsigned dim = m32(dims + d * 4u); if (dim == 0u) return; count *= dim; }
  const __fp16 *in = (const __fp16 *)m32(params);
  __fp16 *out = (__fp16 *)m32(params + 0x50u);
  for (unsigned i = 0u; i < count; ++i) out[i] = in[i] + 1.0f + 1.0f;
}
