/* minimal repro: local array indexed at runtime (alloca/frame) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned scratch[8];
  for (unsigned i = 0; i < 8; ++i) scratch[i] = in[i];
  out[0] = scratch[in[1] & 7u];
}
