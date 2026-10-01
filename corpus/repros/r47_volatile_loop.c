/* minimal repro: volatile loop counter / inline asm barrier effects */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  volatile unsigned sink = 0;
  unsigned acc = 0;
  for (unsigned i = 0; i < 8; ++i) { acc += in[i]; sink = acc; }
  out[0] = sink;
}
