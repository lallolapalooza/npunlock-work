/* corpus synthetic kernel: 64-bit integer arithmetic, shifts, compares, div/mod */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned long long a = ((unsigned long long)in[1] << 32) | in[0];
  unsigned long long b = ((unsigned long long)in[3] << 32) | (in[2] | 1u);
  unsigned long long s = a + b;
  unsigned long long d = a - b;
  unsigned long long m = (a >> 3) + (b << 2);
  unsigned long long p = a * (b | 3ull);
  unsigned long long q = a / (b | 7ull);
  unsigned long long r = a % (b | 7ull);
  long long sa = (long long)a;
  long long sb = (long long)(b >> 1);
  long long sd = sa / (sb | 1);
  out[0] = (unsigned)(s ^ d);
  out[1] = (unsigned)(m >> 16);
  out[2] = (unsigned)p;
  out[3] = (unsigned)(p >> 32);
  out[4] = (unsigned)q + (unsigned)r;
  out[5] = (unsigned)sd;
  out[6] = (a > b) ? 1u : 0u;
}
