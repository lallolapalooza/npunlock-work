/* corpus synthetic kernel: u32/i32 division and modulo by runtime values */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned a = in[0] | 1u;
  unsigned b = (in[1] & 0x1fu) + 3u;
  unsigned udiv = a / b;
  unsigned umod = a % b;
  int sa = (int)a;
  int sb = (int)b;
  int sdiv = sa / sb;
  int smod = sa % sb;
  out[0] = udiv;
  out[1] = umod;
  out[2] = (unsigned)sdiv;
  out[3] = (unsigned)smod;
}
