/* minimal repro: struct copy of 8 words */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
typedef struct big { unsigned v[8]; } big;
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  big a, b;
  for (unsigned i = 0; i < 8; ++i) a.v[i] = in[i];
  b = a;
  out[0] = b.v[in[1] & 7u];
}
