/* minimal repro: i32 -> i64 sign/zero extension and i64 compare */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  long long s = (long long)(int)in[0];
  unsigned long long u = (unsigned long long)in[1];
  out[0] = (unsigned)(s + (long long)u);
  out[1] = (s < (long long)u) ? 1u : 0u;
}
