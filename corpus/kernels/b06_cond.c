/* corpus synthetic kernel: if/else chains, ternary, logical operators */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned v = in[0];
  unsigned r;
  if (v < 10u) r = 1u;
  else if (v < 20u) r = 2u;
  else if (v < 30u) r = 3u;
  else if ((v & 1u) != 0u && v != 0u) r = 4u;
  else if (v == 0u || (v >> 3) == 0u) r = 5u;
  else r = 6u;
  r = (v > 7u) ? (r + 1u) : (r - 1u);
  if (!(v & 0xffff0000u)) r += 0x100u;
  if (v && !(v & 0x0f0f0f0fu)) r += 0x200u;
  out[0] = r;
  out[1] = (v > 5u) + ((v == 5u) << 1) + ((v != 5u) << 2) + ((v <= 5u) << 3);
}
