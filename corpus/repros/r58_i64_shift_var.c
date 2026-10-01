/* minimal repro: 64-bit variable shift amounts */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned long long a = ((unsigned long long)in[1] << 32) | in[0];
  out[0] = (unsigned)(a >> (in[2] & 63u));
}
