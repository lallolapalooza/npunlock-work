/* minimal repro: byte copies lowered to memcpy/memset */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned char *dst = (unsigned char *)out;
  for (unsigned i = 0; i < 40; ++i) dst[i] = (unsigned char)(in[0] + i);
  out[10] = dst[0];
}
