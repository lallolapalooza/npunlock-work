/* minimal repro: struct returned by value from a helper */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
typedef struct s { unsigned a; unsigned b; } s;
static s make(unsigned v) { s r; r.a = v; r.b = v + 1u; return r; }
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  s v = make(in[0]);
  out[0] = v.a + v.b;
}
