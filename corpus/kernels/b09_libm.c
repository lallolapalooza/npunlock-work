/* corpus synthetic kernel: external libm calls (mlibm.a) */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>
void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);
  for (unsigned i = 0; i < invocation.element_count; ++i) {
    float x = (float)in[i];
    float a = tanhf(x);
    float b = expf(x * 0.25f);
    float c = sqrtf(fabsf(x) + 1.0f);
    out[i] = (__fp16)(a + b * c);
  }
}
