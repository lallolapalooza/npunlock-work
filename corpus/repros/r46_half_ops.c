/* minimal repro: fp16 arithmetic and int<->fp16 conversions */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const __fp16 *in = (const __fp16 *)load32((const unsigned char *)layerParams);
  __fp16 *out = (__fp16 *)load32((const unsigned char *)layerParams + 0x28);
  float f = (float)in[0] + (float)in[1];
  out[0] = (__fp16)f;
  out[1] = (__fp16)((int)in[2] * 2);
}
