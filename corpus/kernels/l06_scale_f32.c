/* npunlock corpus kernel, family l06: llama.cpp/ggml vector scale (f32).
 *
 * Provenance: llama.cpp/ggml  llama/vec.h:703 `ggml_vec_scale_f32`
 *             (`for (i) y[i] *= v;`, i.e. y = a*x).
 * External math: none.
 *
 * Documented deviation: ggml takes the scale factor as a runtime argument
 * (`ggml_vec_scale_f32(n, y, v)`); the observed ACT ABI has no op_params, so v
 * is carried in the second input record (a one-element f32 tensor) and the
 * scaled vector is written to the third record.
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
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *x = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  const float *a = ACT_ABI_INPUT_PTR32(const float, invocation, 1u);
  float *y = ACT_ABI_OUTPUT_PTR32(float, invocation, 2u);

  const float v = a[0];
  for (unsigned i = 0; i < invocation.element_count; ++i) {
    y[i] = x[i] * v;
  }
}
