/* minimal repro: double compare that cannot be folded to integers */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const float *in = (const float *)load32((const unsigned char *)layerParams);
  float *out = (float *)load32((const unsigned char *)layerParams + 0x28);
  double a = (double)in[0] * 0.125;
  out[0] = (a > 2.0) ? 1.0f : 0.0f;
}
