/* kernel-lib: rope_f32 -- NEOX-layout rotary position embedding (f32)
 *
 * op:      rope (ggml GGML_OP_ROPE / ggml_compute_forward_rope_flt<float>)
 * records: input 0 = x (f32 activations), input 1 = pos (i32 token positions),
 *          output 0 = y (f32)
 * dtype:   f32
 * shape:   per-token-row.  n_dims = dims32[0] (ne0, the head dimension);
 *          ne1 = dims32[1] (attention heads per token, defaulting to 1);
 *          rows are walked by advancing the base pointer by n_dims, bounded by
 *          invocation.element_count.
 * extern:  sinf, cosf, powf (mlibm)
 *
 * Per row (ggml_rope_cache_init + rotate_pairs):
 *   theta_scale = powf(freq_base, -2/n_dims); theta = freq_scale * pos[token]
 *   for k in [0, n_dims/2):
 *       c = cosf(theta) * mscale; s = sinf(theta) * mscale * sin_sign
 *       y[k]           = x[k] * c - x[k + n_dims/2] * s
 *       y[k + n_dims/2] = x[k] * s + x[k + n_dims/2] * c
 *       theta *= theta_scale
 *
 * Documented deviations, all forced by what the observed ACT ABI carries:
 *  1) n_dims is pinned to ne0 (the whole row is rotated; the `!is_vision`
 *     copy-only tail over [n_offs, n_offs+n_dims) is empty and is omitted);
 *  2) mode is pinned to GGML_ROPE_TYPE_NEOX (n_offset = n_dims/2);
 *  3) freq_base / freq_scale / ext_factor / attn_factor are op_params: llama's
 *     defaults are pinned (10000.0f, 1.0f, 0.0f -- the YaRN ramp term of
 *     rope_yarn is exactly zero -- and attn_factor 1.0f), sin_sign = +1.0f;
 *  4) the rope cache (params->wdata) is not materialised; cos/sin are computed
 *     directly, which is the same arithmetic the cache holds.
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
  const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  const int *pos = ACT_ABI_INPUT_PTR32(const int, invocation, 1u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 2u);

  const int n_dims = (int)act_abi_load_le_u32(invocation.dims32 + 0u); /* ne0 */
  const unsigned ne1 = (invocation.rank >= 2u)
                           ? act_abi_load_le_u32(invocation.dims32 + 4u)
                           : 1u; /* attention heads per token */
  if (n_dims <= 0 || (unsigned)n_dims > invocation.element_count) {
    return;
  }

  const float freq_base = 10000.0f;
  const float freq_scale = 1.0f;
  const float mscale = 1.0f;    /* attn_factor */
  const float sin_sign = 1.0f;  /* forward */
  const float theta_scale = powf(freq_base, -2.0f / (float)n_dims);

  unsigned head = 0;
  unsigned token = 0;
  for (unsigned base = 0; base + (unsigned)n_dims <= invocation.element_count;
       base += (unsigned)n_dims) {
    const float *x = in + base;
    float *y = out + base;

    float theta = freq_scale * (float)pos[token];

    for (int k = 0; k < n_dims / 2; ++k) {
      const float cos_theta = cosf(theta) * mscale;
      const float sin_theta = sinf(theta) * mscale * sin_sign;
      const float x0 = x[k];
      const float x1 = x[k + n_dims / 2];
      y[k] = x0 * cos_theta - x1 * sin_theta;
      y[k + n_dims / 2] = x0 * sin_theta + x1 * cos_theta;
      theta *= theta_scale;
    }

    if (++head == ne1) {
      head = 0;
      ++token;
    }
  }
}
