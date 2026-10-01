/* minimal repro: fp32 add/sub/mul/neg/abs */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const float *in = (const float *)load32((const unsigned char *)layerParams);
  float *out = (float *)load32((const unsigned char *)layerParams + 0x28);
  float a = 1.0f;
  for (unsigned i = 0; i < 8; ++i) {
    float x = in[i];
    a = a + x;
    a = a - x * 0.5f;
    a = a * 1.25f;
    a = -a;
    a = __builtin_fabsf(a);
  }
  out[0] = a;
}
