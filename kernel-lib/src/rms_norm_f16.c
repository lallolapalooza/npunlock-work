/* kernel-lib: rms_norm_f16 -- y = x * (1 / sqrtf(mean(x^2) + eps))
 *
 * op:      rms_norm (ggml GGML_OP_RMS_NORM, f16 storage)
 * records: input 0 = x (f16), output 0 = y (f16)
 * dtype:   f16
 * shape:   row-wise reduction, ne00 = dims32[0]; row loop bounded by
 *          invocation.element_count (invocation-local chunk).
 * extern:  sqrtf (mlibm)
 *
 * Same two documented deviations as rms_norm_f32.c (eps pinned to llama.cpp's
 * 1e-5f, f32 accumulator).  The reduction runs in f32 over the widened
 * elements, which is what ggml's SIMD path does for f16 storage too.
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
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);

  const unsigned ne00 = act_abi_load_le_u32(invocation.dims32 + 0u);
  if (ne00 == 0u || ne00 > invocation.element_count) {
    return;
  }

  for (unsigned base = 0; base + ne00 <= invocation.element_count; base += ne00) {
    const __fp16 *x = in + base;
    __fp16 *y = out + base;

    float sum = 0.0f;
    for (unsigned i00 = 0; i00 < ne00; ++i00) {
      const float v = (float)x[i00];
      sum += v * v;
    }

    const float mean = sum / (float)ne00;
    const float scale = 1.0f / sqrtf(mean + GGML_RMS_NORM_EPS);

    for (unsigned i00 = 0; i00 < ne00; ++i00) {
      y[i00] = (__fp16)((float)x[i00] * scale);
    }
  }
}
