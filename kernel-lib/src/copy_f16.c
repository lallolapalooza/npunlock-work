/* kernel-lib: copy_f16 -- out[i] = in[i] (the canonical copy kernel)
 *
 * op:      copy (ggml GGML_OP_DUP / CPY, f16) -- and the second ACT stage of
 *          every packed ggml-shave engine blob.
 * records: input 0 = in (f16), output 0 = out (f16)
 * dtype:   f16
 * shape:   elementwise; element_count is the invocation-local chunk, so the
 *          same ELF serves every invocation of a patched group.
 *
 * The conversion through float is deliberate: a raw fp16 load/store pair is
 * compiled into the LSU0 store form that the SHAVE write-back rules discard
 * (kernels/shave_copy.c in ggml-shave documents the same measurement), while
 * the float path selects LSU0.LD.32.F16.F32 / LSU1.ST.16.F32.F16.
 *
 * Byte-transparent: patching this kernel into a carrier makes the graph output
 * equal to the ACT's input window, which is how tools/run_npu.py calibrates the
 * exact fp16 values a carrier delivers.
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);

  for (unsigned i = 0; i < invocation.element_count; ++i) {
    out[i] = (__fp16)((float)in[i]);
  }
}
