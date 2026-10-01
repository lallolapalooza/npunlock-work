/* verbatim kernel string from examples/example_gelu_f32.py (gelu_c) */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
    act_abi_invocation invocation;
    ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
    const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
    float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);
    const float SQRT_2_DIV_PI = 0.7978845608028654f;
    for (unsigned i = 0; i < invocation.element_count; ++i) {
      float x = in[i];
      float w = x + 0.044715f * x * x * x;
      w = w * SQRT_2_DIV_PI;
      w = tanhf(w);
      out[i] = 0.5f * x * (1.0f + w);
    }
}
