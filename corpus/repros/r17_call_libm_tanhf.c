#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>
/* minimal repro: call into libm (mlibm.a tanhf) */
float tanhf(float);
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const __fp16 *in = (const __fp16 *)load32((const unsigned char *)layerParams);
  __fp16 *out = (__fp16 *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = (__fp16)tanhf((float)in[0]);
}
