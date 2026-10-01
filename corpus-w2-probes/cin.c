void controlled_act(unsigned layerParams) {
  const unsigned char *params = (const unsigned char *)layerParams;
  unsigned o = (unsigned)params[0x28]|((unsigned)params[0x29]<<8)|((unsigned)params[0x2a]<<16)|((unsigned)params[0x2b]<<24);
  unsigned i = (unsigned)params[0]|((unsigned)params[1]<<8)|((unsigned)params[2]<<16)|((unsigned)params[3]<<24);
  float *out = (float *)o; const float *in = (const float *)i;
  out[0]=in[0]; out[1]=in[1]; out[2]=in[2]; out[3]=in[3]; out[4]=in[4]; out[5]=in[5]; out[6]=in[6]; out[7]=in[7];
}
