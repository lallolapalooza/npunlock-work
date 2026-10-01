/* corpus synthetic kernel: integer multiply, all four paths in one place.
 *
 * The invocation hands us element_count fp16 elements.  Group i reads two
 * 32-bit input words and writes four 32-bit output words (its whole 16-byte
 * output window), and the group count is element_count/8 so the kernel never
 * writes outside its own output window -- two ACT invocations share one
 * 32-byte output tensor, and words written past the window are overwritten by
 * the sibling invocation:
 *   in[2i]    = a, in[2i+1] = b
 *   out[4i]   = high half of the 64-bit product  (IAU.MUL + LowerMULHU)
 *   out[4i+1] = low half of the 64-bit product   (IAU.MUL)
 *   out[4i+2] = plain 32-bit a*b                 (IAU.MUL, control)
 *   out[4i+3] = 0xFFFFFFFFu / (b | 1u)           (constant division -> mulhu)
 *
 * On the 32-byte porting-probe carrier (element_count = 8) it therefore reads
 * its own 16-byte input window and writes its own 16-byte output window, so
 * both invocations can be compared against a host oracle.  This is the numeric
 * regression for the 2026-09-28 integer-multiply defect (wrong IAU.SHR
 * immediate encoding inside LowerMULHU, wrong IAU.SHR.I32 form, and stores
 * emitted on the non-functional LSU0 unit).
 */
#include <npunlock/npu3720_kernel.h>

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const unsigned *in = ACT_ABI_INPUT_PTR32(const unsigned, invocation, 0u);
  unsigned *out = ACT_ABI_OUTPUT_PTR32(unsigned, invocation, 1u);
  unsigned groups = invocation.element_count / 8u;
  for (unsigned i = 0; i < groups; ++i) {
    unsigned a = in[2u * i];
    unsigned b = in[2u * i + 1u];
    unsigned long long p = (unsigned long long)a * (unsigned long long)b;
    out[4u * i] = (unsigned)(p >> 32);
    out[4u * i + 1u] = (unsigned)p;
    out[4u * i + 2u] = a * b;
    out[4u * i + 3u] = 0xFFFFFFFFu / (b | 1u);
  }
}
