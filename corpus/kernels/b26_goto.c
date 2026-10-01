/* corpus synthetic kernel: goto, labels, and unreachable tails */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned acc = 0;
  unsigned i = 0;
loop:
  acc += in[i] & 0xfu;
  if (++i < 8u) goto loop;
  if (acc == 0u) goto done;
  acc ^= 0xaaaaaaaau;
  if ((acc & 1u) != 0u) goto skip;
  acc += 5u;
skip:
  acc *= 3u;
done:
  out[0] = acc;
}
