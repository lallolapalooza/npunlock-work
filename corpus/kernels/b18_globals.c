/* corpus synthetic kernel: static const tables, globals, address materialization */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static const unsigned table[8] = {3u, 5u, 7u, 11u, 13u, 17u, 19u, 23u};
static unsigned counter = 1u;
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned acc = counter;
  for (unsigned i = 0; i < 8; ++i) acc += table[i] * (in[i] & 3u);
  counter = acc | 1u;
  out[0] = acc;
}
