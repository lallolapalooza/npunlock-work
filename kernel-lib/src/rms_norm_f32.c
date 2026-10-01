/* kernel-lib: rms_norm_f32 -- y = x * (1 / sqrtf(mean(x^2) + eps))
 *
 * op:      rms_norm (ggml GGML_OP_RMS_NORM / ggml_compute_forward_rms_norm_f32)
 * records: input 0 = x (f32), output 0 = y (f32)
 * dtype:   f32
 * shape:   row-wise reduction.  ne00 = dims32[0] (innermost dim / row length);
 *          rows are walked by advancing the base pointer, so no runtime integer
 *          division is needed.  Each row is reduced independently; the row loop
 *          is bounded by invocation.element_count, i.e. the invocation-local
 *          chunk only (no cross-chunk halo).
 * extern:  sqrtf (mlibm)
 *
 * Two documented deviations from ggml, both forced by the observed ACT ABI:
 *  1) eps is a ggml op_param; the invocation record does not carry op_params,
 *     so llama.cpp's hparams.f_norm_rms_eps = 1e-5f is pinned.
 *  2) the accumulator is `float`.  ggml uses ggml_float (= double), but mlibm
 *     ships no fp64 soft-float helpers and the target has no fp64 unit;
 *     ggml's own SIMD accumulation path (`float sumf`) is float as well.
 *
 * Provenance: llama.cpp/ggml  llama/ops.cpp:3924, llama/vec.h:703.
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

float sqrtf(float);

#define GGML_RMS_NORM_EPS 1e-5f

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);

  const unsigned ne00 = act_abi_load_le_u32(invocation.dims32 + 0u);
  if (ne00 == 0u || ne00 > invocation.element_count) {
    return;
  }

  for (unsigned base = 0; base + ne00 <= invocation.element_count; base += ne00) {
    const float *x = in + base;
    float *y = out + base;

    float sum = 0.0f;
    for (unsigned i00 = 0; i00 < ne00; ++i00) {
      sum += x[i00] * x[i00];
    }

    const float mean = sum / (float)ne00;
    const float scale = 1.0f / sqrtf(mean + GGML_RMS_NORM_EPS);

    for (unsigned i00 = 0; i00 < ne00; ++i00) {
      y[i00] = x[i00] * scale;
    }
  }
}
