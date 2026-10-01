/* Multi-entry ACT kernel: N independent entry points in one .text image.
   Each entry overwrites its invocation's output tensor with a distinctive
   fp16 bit pattern so we can prove which entry actually executed. */
#include <npunlock/npu3720_kernel.h>

static ACT_ABI_ALWAYS_INLINE unsigned multi_ld32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

#define MULTI_ACT_ENTRY(name, value)                                                     \
  void name(unsigned layerParams) {                                                      \
    const unsigned char *params = (const unsigned char *)layerParams;                    \
    unsigned rank = multi_ld32(params + 0xcu);                                           \
    const unsigned char *dims = (const unsigned char *)multi_ld32(params + 0x10u);       \
    if (rank == 0u || rank > 15u || dims == 0) return;                                   \
    unsigned count = 1u;                                                                 \
    for (unsigned d = 0u; d < rank; ++d) {                                               \
      unsigned dim = multi_ld32(dims + d * 4u);                                          \
      if (dim == 0u || dim > 16u) return;                                                \
      count *= dim;                                                                      \
    }                                                                                    \
    unsigned short *out = (unsigned short *)multi_ld32(params + 0x28u);                  \
    for (unsigned i = 0u; i < count; ++i) out[i] = (unsigned short)(value);              \
  }

MULTI_ACT_ENTRY(controlled_act, 0x3C00u) /* fp16 1.0 */
MULTI_ACT_ENTRY(multi_b, 0x4200u)       /* fp16 3.0 */
MULTI_ACT_ENTRY(multi_c, 0x4400u)       /* fp16 4.0 */
MULTI_ACT_ENTRY(multi_d, 0x4500u)       /* fp16 5.0 */
