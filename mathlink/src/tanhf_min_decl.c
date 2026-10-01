#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>
float tanhf(float);

void controlled_act(unsigned layerParams) {
    act_abi_invocation invocation;
    ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
    const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
    float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);
    for (unsigned i = 0; i < invocation.element_count; ++i) {
      out[i] = tanhf(in[i]);
    }
}
