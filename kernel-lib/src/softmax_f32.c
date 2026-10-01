/* kernel-lib: softmax_f32 -- row-wise softmax
 *
 * op:      softmax (ggml GGML_OP_SOFT_MAX / ggml_compute_forward_soft_max_f32)
 * records: input 0 = x (f32), output 0 = y (f32)
 * dtype:   f32
 * shape:   row-wise reduction.  ne00 = dims32[0] (row length); each row is
 *          independently max-reduced, exponentiated and normalised.  The row
 *          loop is bounded by invocation.element_count (invocation-local chunk).
 * extern:  expf (mlibm)
 *
 * Documented deviations: the logit scale and the optional ALiBi mask are
 * op_params / extra tensors the observed ACT ABI does not carry, so this is the
 * core softmax (max reduce -> expf(x-max) -> sum -> divide).  The sum is
 * `float` for the same reason as rms_norm (no fp64 on this target).
 *
 * Provenance: llama.cpp/ggml  llama/ops.cpp:5584, llama/vec.h:1541/703.
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

float expf(float);

/* ggml-impl.h: MAX(a, b) ((a) > (b) ? (a) : (b)) */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);

  const unsigned ne00 = act_abi_load_le_u32(invocation.dims32 + 0u);
  if (ne00 == 0u || ne00 > invocation.element_count) {
    return;
  }

  for (unsigned base = 0; base + ne00 <= invocation.element_count; base += ne00) {
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

    /* ggml_vec_scale_f32(ne00, y, 1/sum) */
    sum = 1.0f / sum;
    for (unsigned i = 0; i < ne00; ++i) {
      y[i] *= sum;
    }
  }
}
