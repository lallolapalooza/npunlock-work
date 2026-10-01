/* minimal repro: fp32 division by a runtime value */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const float *in = (const float *)load32((const unsigned char *)layerParams);
  float *out = (float *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = in[0] / in[1];
}
