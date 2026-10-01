/* corpus synthetic kernel: fp16<->fp32 and int<->float conversions */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  __fp16 *out16 = (__fp16 *)load32((const unsigned char *)layerParams + 0x28);
  float *out32 = (float *)load32((const unsigned char *)layerParams + 0x50);
  unsigned u = in[0];
  int s = (int)u;
  float f = (float)s;
  float g = (float)u;
  __fp16 h = (__fp16)f;
  f = f + g;
  unsigned back = (unsigned)f;
  f = f + (float)back;
  out16[0] = h;
  out16[1] = (__fp16)(g * 0.5f);
  out32[0] = f;
  out32[1] = (float)h;
}
