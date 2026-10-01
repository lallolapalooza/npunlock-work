/* kernel-lib: mul_f16_packed -- packed-pair f16 multiply (one input record)
 *
 * op:      mul (ggml GGML_OP_MUL, f16, equal shapes) in the packed-tile form a
 *          single-input carrier requires: the host packs [a | b] into one tile
 *          and lane i pairs with lane i ^ element_count.
 * records: input 0 = packed tile (f16, 2 * count elements), output 0 = result
 *          (f16, count elements per invocation)
 * dtype:   f16
 * shape:   elementwise; only the first invocation window of a chained carrier
 *          is meaningful (see README "Carrier contract")
 *
 * fp16 operands on purpose (see mul_f16.c / measured_toolchain_rules D3).
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);

  const unsigned count = invocation.element_count;
  for (unsigned i = 0; i < count; ++i) {
    out[i] = in[i] * in[i ^ count];
  }
}
