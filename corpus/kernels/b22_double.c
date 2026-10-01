/* corpus synthetic kernel: double-precision arithmetic (f64) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  double a = (double)in[0];
  double b = (double)in[1] + 0.5;
  double c = a * b + 3.25;
  c = c / (b + 1.0);
  out[0] = (unsigned)c;
  out[1] = (unsigned)(c > b ? 1 : 0);
}
