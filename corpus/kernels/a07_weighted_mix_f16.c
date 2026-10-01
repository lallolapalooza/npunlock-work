/* verbatim kernel string from examples/example_multilayer_multi_input.py (weighted_mix_c) */
#include <npunlock/npu3720_kernel.h>

/* Calibrated static dense FP16 binary ACT entry.
 *
 * For the validated two-input carrier, the tensor records are:
 *   input A: layerParams + 0x00
 *   input B: layerParams + 0x28
 *   output:  layerParams + 0x50
 *
 * This example is intentionally fixed to the (1, 32) graph below. The UMD
 * partitions its binary carrier into four invocations of eight elements.
 */
void controlled_act(unsigned layerParams) {
    act_abi_invocation invocation;
    ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
    const __fp16 *a = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
    const __fp16 *b = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 1u);
    __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 2u);
    for (unsigned i = 0; i < invocation.element_count; ++i) {
        float lhs = (float)a[i];
        float rhs = (float)b[i];
        out[i] = (__fp16)(lhs * 0.75f + rhs * 0.25f);
    }
}
