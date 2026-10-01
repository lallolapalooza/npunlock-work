/* corpus synthetic kernel: for/while/do-while, nested loops, break and continue */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned acc = 0;
  for (unsigned i = 0; i < 16; ++i) {
    if (in[i] == 0u) continue;
    if (in[i] > 100000u) break;
    unsigned j = i;
    while (j > 0u) { acc += j; --j; if (acc > 0xffff0000u) break; }
    do { acc ^= (acc << 3); acc -= 1u; } while (acc & 0x80000000u);
    for (unsigned k = 0; k < 4; ++k) {
      for (unsigned l = 0; l < 4; ++l) {
        if (l == k) continue;
        acc += k * l;
      }
    }
  }
  out[0] = acc;
  out[1] = in[0];
}
