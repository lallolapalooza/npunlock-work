/* corpus synthetic kernel: signed loop indices, negative offsets, pointer arithmetic */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned char *base = (const unsigned char *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  const int *mid = (const int *)(base + 64);
  unsigned acc = 0;
  for (int i = -8; i < 8; ++i) acc += (unsigned)mid[i];
  const int *p = mid + 4;
  for (int i = 0; i < 8; ++i) acc ^= (unsigned)p[-i];
  long delta = (long)((const unsigned char *)p - base);
  out[0] = acc;
  out[1] = (unsigned)delta;
}
