/* corpus synthetic kernel: volatile accesses (no reordering, no elision) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  volatile unsigned *in = (volatile unsigned *)load32((const unsigned char *)layerParams);
  volatile unsigned *out = (volatile unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned acc = 0;
  for (unsigned i = 0; i < 8; ++i) {
    acc += in[i];
    out[i] = acc;
  }
  out[8] = acc;
}
