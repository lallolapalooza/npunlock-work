float expf(float);
void controlled_act(unsigned layerParams) {
  const unsigned char *params = (const unsigned char *)layerParams;
  unsigned o = (unsigned)params[0x28]|((unsigned)params[0x29]<<8)|((unsigned)params[0x2a]<<16)|((unsigned)params[0x2b]<<24);
  unsigned i = (unsigned)params[0]|((unsigned)params[1]<<8)|((unsigned)params[2]<<16)|((unsigned)params[3]<<24);
  float *out = (float *)o; const float *in = (const float *)i;
  out[0] = expf(in[0]);
  out[1] = in[1];
  out[2] = in[2];
}
