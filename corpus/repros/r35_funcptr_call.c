/* minimal repro: indirect call through a runtime-selected function pointer */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
static unsigned op_a(unsigned v);
static unsigned op_b(unsigned v);
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned (*fn)(unsigned) = (in[1] & 1u) ? op_a : op_b;
  out[0] = fn(in[0]);
}
static unsigned op_a(unsigned v) { return v + 7u; }
static unsigned op_b(unsigned v) { return v ^ 0x55u; }
