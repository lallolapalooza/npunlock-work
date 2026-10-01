/* minimal repro: high register pressure (forces spills) */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned a0 = in[0], a1 = in[1], a2 = in[2], a3 = in[3], a4 = in[4], a5 = in[5], a6 = in[6], a7 = in[7];
  unsigned b0 = a0+1u, b1 = a1+2u, b2 = a2+3u, b3 = a3+4u, b4 = a4+5u, b5 = a5+6u, b6 = a6+7u, b7 = a7+8u;
  unsigned c0 = b0^b1, c1 = b2^b3, c2 = b4^b5, c3 = b6^b7;
  unsigned d0 = c0*c1, d1 = c2*c3;
  unsigned e0 = d0+d1, e1 = d0^d1, e2 = d0|d1, e3 = d0&d1, e4 = d0-d1, e5 = d0*3u;
  unsigned f0 = e0+e1, f1 = e2+e3, f2 = e4+e5;
  unsigned g0 = f0*f1 + f2;
  out[0] = a0+a1+a2+a3+a4+a5+a6+a7;
  out[1] = b0+b1+b2+b3+b4+b5+b6+b7;
  out[2] = c0+c1+c2+c3;
  out[3] = g0;
}
