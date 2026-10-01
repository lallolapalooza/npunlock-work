/* kernel-lib: scale_f16_packed -- packed f16 scale (one input record)
 *
 * op:      scale (ggml GGML_OP_SCALE, f16) for a single-input carrier: the tile
 *          is [x | scalar] and the factor travels in the first lane of the
 *          second half (offset `element_count`).
 * records: input 0 = packed tile (f16, 2 * count), output 0 = result (f16, count)
 * dtype:   f16
 * shape:   elementwise; lanes [0, count) are the vector, tile[count] is the
 *          factor.  ggml's runtime scalar is not carried by the ACT ABI.
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
  const __fp16 v = in[count];
  for (unsigned i = 0; i < count; ++i) {
    out[i] = in[i] * v;
  }
}
