/* minimal repro: initialized global in .data with runtime read */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
static unsigned g_table[4] = {1u, 2u, 3u, 4u};
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  g_table[in[0] & 3u] = in[1];
  out[0] = g_table[0] + g_table[1] + g_table[2] + g_table[3];
}
