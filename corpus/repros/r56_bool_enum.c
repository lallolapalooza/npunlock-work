/* minimal repro: _Bool, enum, unsigned char promotion */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  enum mode { A = 1, B = 5, C = 9 };
  enum mode m = (enum mode)((in[0] % 3u) * 4u + 1u);
  _Bool flag = (in[1] & 1u) != 0u;
  unsigned char c = (unsigned char)in[2];
  unsigned acc = (unsigned)m + (unsigned)c;
  if (flag) acc += 100u;
  out[0] = acc;

}
