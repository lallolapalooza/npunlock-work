/* corpus synthetic kernel: integer arithmetic, shifts, signed/unsigned compares, min/max */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned acc = 0x12345678u;
  for (unsigned i = 0; i < 16; ++i) {
    unsigned v = in[i];
    acc = acc + v * 3u;
    acc = acc - (v >> 2);
    acc = acc + (v << 1);
    acc = acc ^ (v & 0x00ff00ffu);
    acc = acc | (v ^ 0xa5a5a5a5u);
    acc = acc & ~(v + 7u);
    if ((int)acc < (int)v) acc = ~acc;
    if (acc > v) acc -= 1u;
    acc = (acc < v) ? acc : v;
    acc = (int)acc > (int)v ? acc : (acc ^ v);
  }
  out[0] = acc;
  out[1] = acc / 3u;
}
