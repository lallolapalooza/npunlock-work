/* minimal repro: unsigned i32 -> f32 conversion */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  float *out = (float *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = (float)in[0];
}
