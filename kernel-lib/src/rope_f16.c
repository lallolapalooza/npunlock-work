/* kernel-lib: rope_f16 -- NEOX-layout rotary position embedding (f16 storage)
 *
 * op:      rope (ggml GGML_OP_ROPE, f16)
 * records: input 0 = x (f16 activations), input 1 = pos (i32 token positions),
 *          output 0 = y (f16)
 * dtype:   f16
 * shape:   per-token-row, n_dims = dims32[0], ne1 = dims32[1] (heads/token);
 *          row loop bounded by invocation.element_count.
 * extern:  sinf, cosf, powf (mlibm)
 *
 * Rotation math (and all four documented ABI-forced deviations) identical to
 * rope_f32.c; storage is f16, rotated in f32 and rounded once on store.
 *
 * Provenance: llama.cpp/ggml  llama/ops.cpp:5958/5975/6062/6082.
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

float sinf(float);
float cosf(float);
float powf(float, float);

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  const int *pos = ACT_ABI_INPUT_PTR32(const int, invocation, 1u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 2u);

  const int n_dims = (int)act_abi_load_le_u32(invocation.dims32 + 0u); /* ne0 */
  const unsigned ne1 = (invocation.rank >= 2u)
                           ? act_abi_load_le_u32(invocation.dims32 + 4u)
                           : 1u;
  if (n_dims <= 0 || (unsigned)n_dims > invocation.element_count) {
    return;
  }

  const float freq_base = 10000.0f;
  const float freq_scale = 1.0f;
  const float mscale = 1.0f;
  const float sin_sign = 1.0f;
  const float theta_scale = powf(freq_base, -2.0f / (float)n_dims);

  unsigned head = 0;
  unsigned token = 0;
  for (unsigned base = 0; base + (unsigned)n_dims <= invocation.element_count;
       base += (unsigned)n_dims) {
    const __fp16 *x = in + base;
    __fp16 *y = out + base;

    float theta = freq_scale * (float)pos[token];

    for (int k = 0; k < n_dims / 2; ++k) {
      const float cos_theta = cosf(theta) * mscale;
      const float sin_theta = sinf(theta) * mscale * sin_sign;
      const float x0 = (float)x[k];
      const float x1 = (float)x[k + n_dims / 2];
      y[k] = (__fp16)(x0 * cos_theta - x1 * sin_theta);
      y[k + n_dims / 2] = (__fp16)(x0 * sin_theta + x1 * cos_theta);
      theta *= theta_scale;
    }

    if (++head == ne1) {
      head = 0;
      ++token;
    }
  }
}
