/* corpus synthetic kernel: bitfields, unions, packing */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

typedef struct flags { unsigned a : 3; unsigned b : 5; unsigned c : 11; unsigned d : 13; } flags;
typedef union bits { unsigned u; float f; unsigned char b[4]; } bits;
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  flags f;
  f.a = in[0] & 7u;
  f.b = in[0] & 31u;
  f.c = in[1] & 2047u;
  f.d = in[1] & 8191u;
  bits v;
  v.u = in[2];
  out[0] = f.a + f.b + f.c + f.d;
  out[1] = v.b[0] + v.b[1] + v.b[2] + v.b[3];
  out[2] = (unsigned)(v.f * 2.0f);
}
