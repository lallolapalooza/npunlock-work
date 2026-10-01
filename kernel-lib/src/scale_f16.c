/* kernel-lib: scale_f16 -- y[i] = x[i] * a[0] (fp16 arithmetic)
 *
 * op:      scale (ggml GGML_OP_SCALE, f16)
 * records: input 0 = x (f16 vector), input 1 = a (f16 tensor whose element 0 is
 *          the scalar factor), output 0 = y (f16 vector)
 * dtype:   f16
 * shape:   elementwise; ggml's runtime scalar travels in the second record
 *          because the observed ACT ABI carries no op_params
 *
 * fp16 operands on purpose (see mul_f16.c / measured_toolchain_rules D3).
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *x = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  const __fp16 *a = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 1u);
  __fp16 *y = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 2u);

  const __fp16 v = a[0];
  for (unsigned i = 0; i < invocation.element_count; ++i) {
    y[i] = x[i] * v;
  }
}
