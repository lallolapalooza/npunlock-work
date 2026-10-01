/* corpus synthetic kernel: int64<->float conversions and 64-bit compares */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  long long s = (long long)(((unsigned long long)in[1] << 32) | in[0]);
  unsigned long long u = ((unsigned long long)in[3] << 32) | in[2];
  float f = (float)s;
  float g = (float)u;
  double d = (double)s;
  long long back = (long long)(f + 0.5f);
  unsigned long long uback = (unsigned long long)(g + 0.5f);
  out[0] = (unsigned)back;
  out[1] = (unsigned)uback;
  out[2] = (unsigned)((double)back + d);
  out[3] = (s < 0) ? 1u : 0u;
}
