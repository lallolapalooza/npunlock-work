/* minimal repro: signed relational compare (SETLT/SETLE/SETGT/SETGE) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  int a = (int)in[0];
  int b = (int)in[1];
  unsigned r = 0;
  if (a < b) r += 1u;
  out[0] = r + (unsigned)(a <= b) + (unsigned)(a > b) + (unsigned)(a >= b);
}
