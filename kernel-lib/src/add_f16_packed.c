/* kernel-lib: add_f16_packed -- packed-pair f16 add (one input record)
 *
 * op:      add (ggml GGML_OP_ADD, f16, equal shapes) in the packed-tile form
 *          the NPU3720 carrier requires: the host packs [operand a | operand b]
 *          into the carrier's single input tensor and the kernel pairs lane i
 *          with lane i ^ element_count.
 * records: input 0 = packed tile (f16, 2 * count elements),
 *          output 0 = result (f16, count elements per invocation)
 * dtype:   f16
 * shape:   elementwise; each invocation owns a count-element window whose
 *          partner half lives element_count positions away, so a 32-element
 *          tile is served by two 16-element invocations (count = 16).
 *
 * For count = 16 this is the semantics of ggml-shave's verified
 * kernels/shave_add.c (`in[i] + in[i ^ 16]`), but the packing constant is
 * taken from the runtime invocation instead of being hard-coded, so the same
 * ELF serves any tile that is two equal invocations.
 *
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);

  const unsigned count = invocation.element_count;
  for (unsigned i = 0; i < count; ++i) {
    out[i] = (__fp16)((float)in[i] + (float)in[i ^ count]);
  }
}
