/* kernel-lib: add_f16 -- y[i] = a[i] + b[i]
 *
 * op:      add (ggml GGML_OP_ADD, f16, equal shapes -- no broadcast)
 * records: input 0 = a (f16 vector), input 1 = b (f16 vector, same length),
 *          output 0 = y (f16 vector)
 * dtype:   f16
 * shape:   elementwise, both inputs must have identical static shapes
 *
 * MIT License.
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *a = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  const __fp16 *b = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 1u);
  __fp16 *y = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 2u);

  for (unsigned i = 0; i < invocation.element_count; ++i) {
    y[i] = (__fp16)((float)a[i] + (float)b[i]);
  }
}
