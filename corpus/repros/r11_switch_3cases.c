/* minimal repro: switch with 3 sparse cases (if-chain) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned r = 0;
  switch (in[0] & 3u) { case 0: r = 1; break; case 1: r = 2; break; default: r = 3; break; }
  out[0] = r;
}
