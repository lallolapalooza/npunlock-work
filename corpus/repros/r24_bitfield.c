/* minimal repro: bitfield struct extract/insert */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
typedef struct bf { unsigned a : 4; unsigned b : 12; } bf;
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  bf f;
  f.a = in[0] & 15u;
  f.b = in[1] & 4095u;
  out[0] = f.a + f.b;
}
