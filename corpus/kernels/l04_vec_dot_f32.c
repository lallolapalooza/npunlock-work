/* npunlock corpus kernel, family l04: llama.cpp/ggml f32 dot product.
 *
 * Provenance: llama.cpp/ggml  llama/vec.cpp:11 `ggml_vec_dot_f32`
 *             (scalar reference: `for (i) sumf += (ggml_float)(x[i]*y[i]);`).
 * External math: none.
 *
 * Documented deviation: the accumulator is `float` (ggml types it ggml_float =
 * double); mlibm ships no fp64 soft-float helpers and the target has no fp64
 * unit (see l02).  ggml's own SIMD accumulation path is float as well.
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

/* Two float inputs (x, y, one element record each) reduce into a one-element
 * output record. */
void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *x = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  const float *y = ACT_ABI_INPUT_PTR32(const float, invocation, 1u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 2u);

  float sumf = 0.0f;
  for (unsigned i = 0; i < invocation.element_count; ++i) {
    sumf += x[i] * y[i];
  }

  *out = sumf;
}
