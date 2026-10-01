/* kernel-lib: silu_f16 -- y[i] = x[i] / (1 + expf(-x[i]))
 *
 * op:      silu (ggml GGML_OP_SILU, f16 storage)
 * records: input 0 = x (f16), output 0 = y (f16)
 * dtype:   f16
 * shape:   elementwise
 * extern:  expf (mlibm)
 *
 * The f32 activation kernel silu_f32.c wrapped for f16 tensors; arithmetic in
 * f32, values rounded once on store.
 *
 * Provenance: llama.cpp/ggml  llama/vec.h:1046 (ggml_silu_f32).
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

float expf(float);

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);

  for (unsigned i = 0; i < invocation.element_count; ++i) {
    const float x = (float)in[i];
    out[i] = (__fp16)(x / (1.0f + expf(-x)));
  }
}
