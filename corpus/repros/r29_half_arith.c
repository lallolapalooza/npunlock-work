/* minimal repro: native __fp16 arithmetic */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const __fp16 *in = (const __fp16 *)load32((const unsigned char *)layerParams);
  __fp16 *out = (__fp16 *)load32((const unsigned char *)layerParams + 0x28);
  out[0] = in[0] + in[1];
}
