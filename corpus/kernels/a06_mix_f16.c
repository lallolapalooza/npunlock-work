/* verbatim kernel string from examples/example_mixed_precision_multi_custom.py (mix_f16_c) */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
    act_abi_invocation invocation;
    ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
    const __fp16 *a = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
    const __fp16 *b = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 1u);
    __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 2u);
    for (unsigned i = 0; i < invocation.element_count; ++i) {
        out[i] = (__fp16)((float)a[i] * 0.75f + (float)b[i] * 0.25f);
    }
}
