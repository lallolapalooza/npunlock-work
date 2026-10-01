/* corpus synthetic kernel: small structs, struct copies, arrays of structs */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

typedef struct pair { unsigned a; unsigned b; } pair;
typedef struct blob { float x; float y; unsigned tag; } blob;
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  pair p;
  p.a = in[0];
  p.b = in[1];
  pair q = p;
  q.a += q.b;
  blob z;
  z.x = (float)p.a;
  z.y = (float)q.a;
  z.tag = p.b ^ q.b;
  out[0] = q.a + z.tag;
  out[1] = (unsigned)(z.x * z.y);
  out[2] = (unsigned)sizeof(pair) + (unsigned)sizeof(blob);
}
