/* minimal repro: fp16<->fp32 round trip through a buffer */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const __fp16 *in = (const __fp16 *)load32((const unsigned char *)layerParams);
  float *out = (float *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = (float)in[0] * 2.0f;
  out[1] = (float)(__fp16)((float)in[1] + 0.5f);
}
