/* corpus synthetic kernel: 64-bit constants and magic-number materialization */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned long long a = 0x123456789abcdefull;
  unsigned long long b = 0xfedcba9876543210ull;
  unsigned long long c = a ^ b;
  float f = 3.14159265358979f;
  double d = 2.718281828459045;
  unsigned x = in[0];
  out[0] = (unsigned)(c >> 32);
  out[1] = (unsigned)(x * 0x9e3779b9u + 0x7f4a7c15u);
  out[2] = (unsigned)(f * 2.0f);
  out[3] = (unsigned)d;
}
