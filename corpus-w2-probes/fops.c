void controlled_act(unsigned layerParams) {
  const unsigned char *params = (const unsigned char *)layerParams;
  unsigned o = (unsigned)params[0x28]|((unsigned)params[0x29]<<8)|((unsigned)params[0x2a]<<16)|((unsigned)params[0x2b]<<24);
  unsigned i = (unsigned)params[0]|((unsigned)params[1]<<8)|((unsigned)params[2]<<16)|((unsigned)params[3]<<24);
  float *out = (float *)o; const float *in = (const float *)i;
  const float a = in[4], b = in[5];
  out[0] = a + b;
  out[1] = a * b;
  out[2] = a - b;
  out[3] = a / b;
  out[4] = a * b + a;
  out[5] = 2.0f * a;
  out[6] = a + 1.0f;
  out[7] = a + 2.0f;
}
