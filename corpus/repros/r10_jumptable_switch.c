/* minimal repro: switch with irregular case values (jump table) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned r = 0;
  switch (in[0] & 7u) {
    case 0: r = 1; break; case 1: r = 3; break; case 2: r = 5; break;
    case 3: r = 11; break; case 4: r = 17; break; case 5: r = 29; break;
    case 6: r = 41; break; default: r = 43; break;
  }
  out[0] = r;
}
