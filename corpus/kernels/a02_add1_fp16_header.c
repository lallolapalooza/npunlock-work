/* verbatim inline source from tests/integration/test_shavecc.c */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>
void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);
  for (unsigned i = 0; i < invocation.element_count; ++i) out[i] = in[i] + 1.0f;
}
