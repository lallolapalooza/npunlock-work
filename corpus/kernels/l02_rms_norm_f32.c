/* npunlock corpus kernel, family l02: llama.cpp/ggml RMSNorm (f32).
 *
 * Provenance: llama.cpp/ggml  llama/ops.cpp:3924
 *             `ggml_compute_forward_rms_norm_f32` (sum of squares -> mean ->
 *             scale = 1.0f/sqrtf(mean + eps) -> y = x*scale, where the copy +
 *             scale is llama/vec.h:703 `ggml_vec_scale_f32`).
 * External math: sqrtf  (lib: mlibm).
 *
 * Two documented deviations, both forced by what the observed ACT ABI carries:
 *  1) eps is a ggml op_param (`memcpy(&eps, dst_rms_norm->op_params, ...)`); the
 *     invocation record carries no op_params, so the llama.cpp default
 *     hparams.f_norm_rms_eps = 1e-5f is pinned as a compile-time constant.
 *  2) the accumulator is `float`.  ggml uses `ggml_float sum` (= double here),
 *     but the SHAVE mlibm ships no fp64 soft-float helpers (no __adddf3 /
 *     __extendsfdf2 / __truncdfsf2) and the target has no fp64 unit; ggml's own
 *     SIMD accumulation paths (vec.cpp `float sumf = 0.0f`) are float as well.
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

float sqrtf(float); /* see l01 note: no <math.h> on this target */

/* llama.cpp hparams.f_norm_rms_eps; ggml takes it from op_params (see header). */
#define GGML_RMS_NORM_EPS 1e-5f

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);

  /* ne00 = row length (innermost dim); rows are walked by advancing the base
   * pointer, exactly like ggml's i01/i02/i03 row loop, so that no runtime
   * integer division is needed. */
  const unsigned ne00 = act_abi_load_le_u32(invocation.dims32 + 0u);
  for (unsigned base = 0; base < invocation.element_count; base += ne00) {
    const float *x = in + base;
    float *y = out + base;

    float sum = 0.0f;
    for (unsigned i00 = 0; i00 < ne00; ++i00) {
      sum += x[i00] * x[i00];
    }

    const float mean  = sum/ne00;
    const float scale = 1.0f/sqrtf(mean + GGML_RMS_NORM_EPS);

    /* memcpy(y, x, ...) + ggml_vec_scale_f32(ne00, y, scale) */
    for (unsigned i00 = 0; i00 < ne00; ++i00) {
      y[i00] = x[i00] * scale;
    }
  }
}
