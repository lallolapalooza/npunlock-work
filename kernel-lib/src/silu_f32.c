/* kernel-lib: silu_f32 -- y[i] = x[i] / (1 + expf(-x[i]))
 *
 * op:      silu (ggml GGML_OP_SILU / ggml_silu_f32)
 * records: input 0 = x (f32 vector), output 0 = y (f32 vector)
 * dtype:   f32
 * shape:   elementwise
 * extern:  expf (mlibm) -- bit-exact on the NPU (FINDINGS.md)
 *
 * Provenance: llama.cpp/ggml  llama/vec.h:1046 `ggml_silu_f32` (vector wrapper
 *             llama/vec.cpp:412).  No <math.h> is reachable from this target, so
 *             the mlibm entry point is declared explicitly (C99 signature).
 *
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

float expf(float);

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);

  for (unsigned i = 0; i < invocation.element_count; ++i) {
    const float x = in[i];
    out[i] = x / (1.0f + expf(-x));
  }
}
