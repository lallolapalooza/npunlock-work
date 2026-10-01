/* kernel-lib: vec_dot_f16 -- out[0] = sum_i x[i] * y[i] (fp16 products)
 *
 * op:      vec_dot (ggml GGML_OP_MUL_MAT inner-product step, f16)
 * records: input 0 = x (f16), input 1 = y (f16, same length),
 *          output 0 = scalar f16 tensor (1 element)
 * dtype:   f16
 * shape:   full reduction over the invocation-local chunk
 *
 * The products are formed on __fp16 operands (the only fp32 multiply form the
 * current toolchain computes, see mul_f16.c) and accumulated in fp32, so the
 * accumulation does not lose the fp16 head-room.  Precision: each product is
 * rounded to fp16 before it is added (measured_toolchain_rules D3).
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *x = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  const __fp16 *y = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 1u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 2u);

  float sumf = 0.0f;
  for (unsigned i = 0; i < invocation.element_count; ++i) {
    sumf += (float)(x[i] * y[i]);
  }

  *out = (__fp16)sumf;
}
