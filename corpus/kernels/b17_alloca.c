/* corpus synthetic kernel: stack array with dynamic indexing (alloca / frame index) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned scratch[16];
  unsigned n = in[0] & 15u;
  for (unsigned i = 0; i < 16; ++i) scratch[i] = in[i + 1] ^ i;
  unsigned acc = 0;
  for (unsigned i = 0; i < n; ++i) acc += scratch[(i * 3u) & 15u];
  out[0] = acc + scratch[n];
}
