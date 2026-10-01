/* kernel-lib: mul_f16 -- y[i] = a[i] * b[i] (fp16 arithmetic)
 *
 * op:      mul (ggml GGML_OP_MUL, f16, equal shapes -- no broadcast)
 * records: input 0 = a (f16), input 1 = b (f16, same length), output 0 = y (f16)
 * dtype:   f16
 * shape:   elementwise; both inputs must have identical static shapes
 *
 * The multiply is written on __fp16 operands on purpose.  The compiler emits
 * fp32 MUL of two *runtime* `float` operands without the `; fmt=2` register
 * hint, which the silicon decodes as a hardware-constant selector (measured:
 * `(float)a[i] * (float)b[i]` returns one of the operands), whereas fp16-typed
 * arithmetic is emitted with the hint and computes the product exactly
 * (reports/npu-evidence.json, measured_toolchain_rules D3).  Byte-identical to
 * the fp32 path except for the final rounding to fp16.
 *
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *a = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  const __fp16 *b = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 1u);
  __fp16 *y = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 2u);

  for (unsigned i = 0; i < invocation.element_count; ++i) {
    y[i] = a[i] * b[i];
  }
}
