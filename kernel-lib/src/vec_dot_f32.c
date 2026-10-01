/* kernel-lib: vec_dot_f32 -- out[0] = sum_i x[i] * y[i]
 *
 * op:      vec_dot (ggml GGML_OP_MUL_MAT inner-product step /
 *          ggml_vec_dot_f32)
 * records: input 0 = x (f32 vector), input 1 = y (f32 vector, same length),
 *          output 0 = scalar f32 tensor (1 element)
 * dtype:   f32
 * shape:   full reduction over the invocation-local chunk
 *          (invocation.element_count elements).
 *
 * Documented deviation: the accumulator is `float`; ggml types it ggml_float
 * (= double), but the target has no fp64 unit and mlibm ships no fp64
 * soft-float helpers.  ggml's own SIMD accumulation path is float as well.
 *
 * Provenance: llama.cpp/ggml  llama/vec.cpp:11 `ggml_vec_dot_f32`
 *             (`for (i) sumf += (ggml_float)(x[i]*y[i]);`).
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#include <npunlock/npu3720_kernel.h>

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
