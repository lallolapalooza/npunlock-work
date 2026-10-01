/* minimal repro: string literal in .rodata referenced at runtime */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  const char *s = "shave";
  unsigned acc = 0;
  for (unsigned i = 0; i < 5; ++i) acc = acc * 31u + (unsigned char)s[i];
  out[0] = acc;
}
