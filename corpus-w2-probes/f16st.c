#include <npunlock/npu3720_kernel.h>
void controlled_act(unsigned layerParams) {
  act_abi_invocation inv;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, inv);
  const float *in = ACT_ABI_INPUT_PTR32(const float, inv, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, inv, 1u);
  for (unsigned i = 0; i < inv.element_count; ++i)
    out[i] = (__fp16)((float)in[i]);
}
