/* kernel-lib: mul_f32 -- y[i] = a[i] * b[i]
 *
 * op:      mul (ggml GGML_OP_MUL, equal shapes -- no broadcast)
 * records: input 0 = a (f32 vector), input 1 = b (f32 vector, same length),
 *          output = y (f32 vector, element_count elements)
 * dtype:   f32
 * shape:   elementwise, both inputs must have identical static shapes
 *
 * MIT License.
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *a = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  const float *b = ACT_ABI_INPUT_PTR32(const float, invocation, 1u);
  float *y = ACT_ABI_OUTPUT_PTR32(float, invocation, 2u);

  for (unsigned i = 0; i < invocation.element_count; ++i) {
    y[i] = a[i] * b[i];
  }
}
