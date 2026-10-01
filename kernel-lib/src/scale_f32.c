/* kernel-lib: scale_f32 -- y[i] = x[i] * a[0]
 *
 * op:      scale (ggml GGML_OP_SCALE / ggml_vec_scale_f32)
 * records: input 0 = x (f32 vector), input 1 = a (f32 tensor whose element 0 is
 *          the scalar factor), output = y (f32 vector, element_count elements)
 * dtype:   f32
 * shape:   elementwise, one output element per invocation-local input element
 *
 * Provenance: llama.cpp/ggml  llama/vec.h:703 `ggml_vec_scale_f32`
 *             (`for (i) y[i] *= v;`).  The observed ACT ABI carries no
 *             op_params, so ggml's runtime scalar `v` travels in the second
 *             input record (see corpus/kernels/l06_scale_f32.c).
 *
 * MIT License; copyright (c) 2023-2026 The ggml authors.
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
