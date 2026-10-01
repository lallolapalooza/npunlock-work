/* npunlock corpus kernel, family l03: llama.cpp/ggml softmax (f32).
 *
 * Provenance: llama.cpp/ggml  llama/ops.cpp:5584
 *             `ggml_compute_forward_soft_max_f32` -> llama/vec.h:1541
 *             `ggml_vec_max_f32` (max reduce) + llama/vec.cpp:531
 *             `ggml_vec_soft_max_f32` (expf(x-max), sum) +
 *             llama/vec.h:703 `ggml_vec_scale_f32` (divide by the sum).
 * External math: expf  (lib: mlibm).
 *
 * Documented deviations:
 *  1) the logit scale and the optional ALiBi mask (`ggml_vec_scale_f32(ne00, wp,
 *     scale)` / `wp[i] += slope*mp[i]`) are op_params/extra tensors that the
 *     observed ACT ABI does not carry, so this kernel is the core softmax:
 *     max-reduce, expf(x-max), sum, divide.
 *  2) the sum is `float`.  ggml uses `ggml_float sum` (= double); mlibm ships no
 *     fp64 soft-float helpers and the target has no fp64 unit (see l02).
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

float expf(float); /* see l01 note: no <math.h> on this target */

/* ggml-impl.h: MAX(a, b) ((a) > (b) ? (a) : (b)) */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* <math.h> is unavailable (see l01), so INFINITY comes from the compiler. */
#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);

  const unsigned ne00 = act_abi_load_le_u32(invocation.dims32 + 0u);
  for (unsigned base = 0; base < invocation.element_count; base += ne00) {
    const float *x = in + base;
    float *y = out + base;

    /* ggml_vec_max_f32 */
    float max = -INFINITY;
    for (unsigned i = 0; i < ne00; ++i) {
      max = MAX(max, x[i]);
    }

    /* ggml_vec_soft_max_f32 */
    float sum = 0.0f;
    for (unsigned i = 0; i < ne00; ++i) {
      const float val = expf(x[i] - max);
      sum += val;
      y[i] = val;
    }

    /* sum = 1.0/sum; ggml_vec_scale_f32(ne00, dp, sum) */
    sum = 1.0f/sum;
    for (unsigned i = 0; i < ne00; ++i) {
      y[i] *= sum;
    }
  }
}
