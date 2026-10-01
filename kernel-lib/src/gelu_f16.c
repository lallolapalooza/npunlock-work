/* kernel-lib: gelu_f16 -- tanh-approximation GELU (fp16 arithmetic)
 *
 * op:      gelu (ggml GGML_OP_GELU, f16)
 * records: input 0 = x (f16), output 0 = y (f16)
 * dtype:   f16
 * shape:   elementwise
 * extern:  tanhf (mlibm)
 *
 *   w = SQRT_2_DIV_PI * (x + 0.044715 * x^3);  y = 0.5 * x * (1 + tanhf(w))
 *
 * Why the arithmetic is spelled out this way (each step is a measured rule of
 * the current toolchain, see reports/npu-evidence.json measured_toolchain_rules):
 *   * the polynomial is evaluated on __fp16 operands, because a *float*
 *     multiply of two runtime operands does not compute (D3), while fp16-typed
 *     arithmetic is emitted with the working register form;
 *   * `1 + tanhf(w)` is evaluated in float (a plain f32 literal add, the same
 *     shape silu_f16 uses) -- writing it as `(__fp16)1.0 + t` makes the
 *     compiler emit the register form for a constant it never materialises
 *     (1.0 is a hardware selector), which silently computes `t + <stale reg>`;
 *   * the final `0.5 * x * (1 + t)` is again fp16 so no fp32 multiply is
 *     involved.
 *
 * Precision: the intermediate polynomial and the final product are rounded to
 * binary16 (about 1e-3 relative) where ggml evaluates the same expression in
 * fp32; x^3 must stay inside the fp16 range (|x| <~ 500).
 *
 * Provenance: examples/example_gelu.py (verbatim kernel string) =
 *             corpus/kernels/a03_gelu_fp16.c.
 * MIT License; copyright (c) 2023-2026 The ggml authors.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);

  const __fp16 C_CUBIC = (__fp16)0.044715;
  const __fp16 SQRT_2_DIV_PI = (__fp16)0.7978845608;
  const __fp16 HALF = (__fp16)0.5;

  for (unsigned i = 0; i < invocation.element_count; ++i) {
    const __fp16 x = in[i];
    const __fp16 w = SQRT_2_DIV_PI * (x + C_CUBIC * x * x * x);
    const __fp16 hx = HALF * x;               /* 0.5*x, before the call */
    const float t = tanhf((float)w);          /* mlibm call, f32 in/out */
    const __fp16 th = (__fp16)t;
    /* 0.5*x*(1+t) == hx + hx*t, spelled without any constant after the call:
     * a constant operand there is emitted in the register form without being
     * materialised (1.0 is a hardware selector), which computes t + <stale>. */
    out[i] = hx + hx * th;
  }
}
