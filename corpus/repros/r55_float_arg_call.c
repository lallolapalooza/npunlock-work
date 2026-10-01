/* minimal repro: call passing float arguments and returning float */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
static float scale(float x, float y);
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = (unsigned)scale((float)in[0], (float)in[1]);
}
static float scale(float x, float y) { return x * y + 1.0f; }
