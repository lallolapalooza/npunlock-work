/* corpus synthetic kernel: byte-initialisation and struct/array copy idioms */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

typedef struct rec { unsigned a; unsigned b; unsigned c; unsigned d; } rec;
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned char *outb = (unsigned char *)load32((const unsigned char *)layerParams + 0x28);
  unsigned char buf[32];
  for (unsigned i = 0; i < 32; ++i) buf[i] = (unsigned char)(in[0] + i);
  rec r = {(unsigned)buf[0], (unsigned)buf[1], (unsigned)buf[2], (unsigned)buf[3]};
  rec copy = r;
  for (unsigned i = 0; i < 32; ++i) outb[i] = buf[i] ^ (unsigned char)copy.d;
}
