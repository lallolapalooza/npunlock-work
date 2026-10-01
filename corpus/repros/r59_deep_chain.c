/* minimal repro: deep if/else chain (many compares and branches) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned v = in[0];
  unsigned r = 0;
  if (v < 1u) r = 1;
  else if (v < 2u) r = 2;
  else if (v < 4u) r = 4;
  else if (v < 8u) r = 8;
  else if (v < 16u) r = 16;
  else if (v < 32u) r = 32;
  else if (v < 64u) r = 64;
  else if (v < 128u) r = 128;
  else if (v < 256u) r = 256;
  else if (v < 512u) r = 512;
  else if (v < 1024u) r = 1024;
  else r = 2048;
  out[0] = r;

}
