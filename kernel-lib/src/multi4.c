/* kernel-lib: multi4 -- four kernels hosted in one ACT image.
 *
 * The NPU3720 graph addresses ACT code per *range*, and the range's code
 * address is rounded down to a 0x400-byte boundary (measured by the multiop
 * workstream), so an image whose entries sit 0x400 bytes apart can run a
 * *different* kernel per range inside one graph invocation.  src/kernel_quad.ld
 * places these four entries at 0x0 / 0x400 / 0x800 / 0xc00; tools/patch_ranges.py
 * then points each ACT range at the entry it should run.
 *
 * Layout (must match kernel_quad.ld):
 *   +0x000  controlled_act  copy_f16        (out = in)
 *   +0x400  kb_add_packed   packed f16 add  (out = in[i] + in[i ^ count])
 *   +0x800  kb_mul_packed   packed f16 mul  (out = in[i] * in[i ^ count])
 *   +0xc00  kb_silu         silu_f16        (out = x / (1 + expf(-x)))
 *
 * Every entry keeps the ACT ABI contract of the single kernels (records at
 * params + 0x28*k, element_count = product(dims)).  The entries mirror the
 * single kernels src/copy_f16.c, src/add_f16_packed.c, src/mul_f16_packed.c and
 * src/silu_f16.c; kb_add_packed evaluates the add on __fp16 operands (the form
 * that needs no asm rewrite) and is element-for-element equal to the fp32 form
 * of add_f16_packed on the vectors measured in reports/npu-evidence.json.
 *
 * The fp16 arithmetic mirrors src/mul_f16_packed.c (measured_toolchain_rules
 * D1/D2/D3 in manifest.json); the two packed entries expect the host to pack
 * [a | b] into the single input tile.
 */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>

float expf(float);

/* +0x000: copy */
void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);
  for (unsigned i = 0; i < invocation.element_count; ++i) {
    out[i] = (__fp16)((float)in[i]);
  }
}

/* +0x400: packed add */
void kb_add_packed(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);
  const unsigned count = invocation.element_count;
  for (unsigned i = 0; i < count; ++i) {
    /* fp16 arithmetic: the compiler annotates the register form itself, so
     * this image needs no asm rewrite (see src/multi4.opts) */
    out[i] = in[i] + in[i ^ count];
  }
}

/* +0x800: packed multiply */
void kb_mul_packed(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);
  const unsigned count = invocation.element_count;
  for (unsigned i = 0; i < count; ++i) {
    out[i] = in[i] * in[i ^ count];
  }
}

/* +0xc00: silu */
void kb_silu(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const __fp16 *in = ACT_ABI_INPUT_PTR32(const __fp16, invocation, 0u);
  __fp16 *out = ACT_ABI_OUTPUT_PTR32(__fp16, invocation, 1u);
  for (unsigned i = 0; i < invocation.element_count; ++i) {
    const float x = (float)in[i];
    out[i] = (__fp16)(x / (1.0f + expf(-x)));
  }
}
