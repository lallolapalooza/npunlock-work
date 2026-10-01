/* corpus synthetic kernel: 8/16/32-bit loads and stores, signed and unsigned */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned char *base = (const unsigned char *)load32((const unsigned char *)layerParams);
  unsigned char *outb = (unsigned char *)load32((const unsigned char *)layerParams + 0x28);
  unsigned short *outh = (unsigned short *)(outb + 32);
  const signed char *sb = (const signed char *)base;
  const unsigned short *uh = (const unsigned short *)(base + 16);
  const short *sh = (const short *)(base + 32);
  unsigned acc = 0;
  for (unsigned i = 0; i < 16; ++i) {
    acc += (unsigned)sb[i];
    acc ^= (unsigned)uh[i];
    acc += (unsigned)sh[i];
    outb[i] = (unsigned char)(acc + i);
  }
  for (unsigned i = 0; i < 8; ++i) outh[i] = (unsigned short)(acc * i);
}
