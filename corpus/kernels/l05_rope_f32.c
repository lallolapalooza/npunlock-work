/* npunlock corpus kernel, family l05: llama.cpp/ggml RoPE (f32, NEOX layout).
 *
 * Provenance: llama.cpp/ggml  llama/ops.cpp:5958 `rope_yarn`,
 *             llama/ops.cpp:5975 `ggml_rope_cache_init`,
 *             llama/ops.cpp:6062 `rotate_pairs<T>`,
 *             llama/ops.cpp:6082 `ggml_compute_forward_rope_flt<float>`.
 * External math: sinf, cosf, powf  (lib: mlibm).
 *
 * The kernel applies, per token row, the cache-init + rotate_pairs pair:
 *   theta_scale = powf(freq_base, -2.0f/n_dims);   theta = (float)pos[token]
 *   for k in [0, n_dims/2):
 *       cos_theta = cosf(freq_scale*theta)*mscale      (rope_yarn)
 *       sin_theta = sinf(freq_scale*theta)*mscale*sin_sign
 *       y[k]            = x[k]*cos_theta - x[k+n_dims/2]*sin_theta
 *       y[k+n_dims/2]   = x[k]*sin_theta + x[k+n_dims/2]*cos_theta
 *       theta *= theta_scale
 * Documented deviations, all forced by what the observed ACT ABI carries:
 *  1) n_dims (= `((int32_t *) dst->op_params)[1]`) is not carried; the kernel
 *     pins n_dims = ne0 (the whole row is rotated, so the `!is_vision` remain
 *     copy loop over [n_offs, n_offs+n_dims) is empty and is omitted).
 *  2) mode is pinned to GGML_ROPE_TYPE_NEOX (n_offset = n_dims/2), which is what
 *     llama uses for its attention heads.
 *  3) freq_base / freq_scale / ext_factor / attn_factor are op_params: the llama
 *     defaults are pinned (10000.0f, 1.0f, 0.0f -> the YaRN ramp term of
 *     rope_yarn is exactly zero, and attn_factor 1.0f) and sin_sign = +1.0f for
 *     the forward pass.
 *  4) the rope cache (`params->wdata`) is not materialised; cos/sin are computed
 *     directly, which is the same arithmetic the cache holds.
 *
 * MIT License
 *
 * Copyright (c) 2023-2026 The ggml authors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

/* see l01 note: no <math.h> on this target */
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

  const float freq_base  = 10000.0f;
  const float freq_scale = 1.0f;
  const float mscale     = 1.0f; /* attn_factor */
  const float sin_sign   = 1.0f; /* forward */
  const float theta_scale = powf(freq_base, -2.0f/n_dims);

  unsigned head = 0;
  unsigned token = 0;
  for (unsigned base = 0; base < invocation.element_count; base += (unsigned)n_dims) {
    const float *x = in + base;
    float *y = out + base;

    /* ggml_rope_cache_init: theta = theta_base * theta_scale^k, w/ theta_base =
     * pos[token]; rope_yarn with ext_factor == 0 gives theta = freq_scale*theta. */
    float theta = (float)pos[token];
    theta = freq_scale * theta;

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
