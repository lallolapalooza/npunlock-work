/* corpus synthetic kernel: indirect calls through a function pointer table */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

static unsigned op_add(unsigned a, unsigned b) { return a + b; }
static unsigned op_mul(unsigned a, unsigned b) { return a * b; }
static unsigned op_xor(unsigned a, unsigned b) { return a ^ b; }
typedef unsigned (*binop)(unsigned, unsigned);
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  static binop ops[3] = {op_add, op_mul, op_xor};
  unsigned acc = in[0];
  for (unsigned i = 0; i < 8; ++i) acc = ops[i % 3u](acc, in[i + 1]);
  out[0] = acc;
}
