/* minimal repro: signed min/max idiom */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  int a = (int)in[0], b = (int)in[1];
  out[0] = (unsigned)(a < b ? a : b) + (unsigned)(a > b ? a : b);
}
