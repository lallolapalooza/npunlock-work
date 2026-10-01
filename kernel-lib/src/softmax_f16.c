/* kernel-lib: softmax_f16 -- row-wise softmax (f16 storage)
 *
 * op:      softmax (ggml GGML_OP_SOFT_MAX, f16)
 * records: input 0 = x (f16), output 0 = y (f16)
 * dtype:   f16
 * shape:   row-wise reduction, ne00 = dims32[0]; row loop bounded by
 *          invocation.element_count.
 * extern:  expf (mlibm)
 *
 * Same deviations as softmax_f32.c (no op_params logit scale / ALiBi mask, f32
 * accumulator).
 *
 * Provenance: llama.cpp/ggml  llama/ops.cpp:5584, llama/vec.h:1541/703.
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

float expf(float);

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);

  const unsigned ne00 = act_abi_load_le_u32(invocation.dims32 + 0u);
  if (ne00 == 0u || ne00 > invocation.element_count) {
    return;
  }

  for (unsigned base = 0; base + ne00 <= invocation.element_count; base += ne00) {
    const __fp16 *x = in + base;
    __fp16 *y = out + base;

    float max = -INFINITY;
    for (unsigned i = 0; i < ne00; ++i) {
      max = MAX(max, (float)x[i]);
    }

    float sum = 0.0f;
    for (unsigned i = 0; i < ne00; ++i) {
      const float val = expf((float)x[i] - max);
      sum += val;
      y[i] = (__fp16)val;
    }

    sum = 1.0f / sum;
    for (unsigned i = 0; i < ne00; ++i) {
      y[i] = (__fp16)((float)y[i] * sum);
    }
  }
}
