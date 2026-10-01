void controlled_act(unsigned layerParams) {
  const unsigned char *params = (const unsigned char *)layerParams;
  unsigned o = (unsigned)params[0x28]|((unsigned)params[0x29]<<8)|((unsigned)params[0x2a]<<16)|((unsigned)params[0x2b]<<24);
  unsigned i = (unsigned)params[0]|((unsigned)params[1]<<8)|((unsigned)params[2]<<16)|((unsigned)params[3]<<24);
  unsigned *out = (unsigned *)o; const unsigned *in = (const unsigned *)i;
  unsigned a = in[0], b = in[1];
  out[0] = a*b;
  out[1] = a; out[2] = b;
}
