void controlled_act(unsigned layerParams) {
  const unsigned char *params = (const unsigned char *)layerParams;
  unsigned o = (unsigned)params[0x28]|((unsigned)params[0x29]<<8)|((unsigned)params[0x2a]<<16)|((unsigned)params[0x2b]<<24);
  unsigned i = (unsigned)params[0]|((unsigned)params[1]<<8)|((unsigned)params[2]<<16)|((unsigned)params[3]<<24);
  unsigned *out = (unsigned *)o; const unsigned *in = (const unsigned *)i;
  unsigned a = in[0], b = in[1];
  unsigned long long p = (unsigned long long)a * (unsigned long long)b;
  out[0] = (unsigned)p;             /* low 32 of the 64-bit product */
  out[1] = (unsigned)(p >> 32);     /* high 32 */
  out[2] = a * b;                   /* plain 32-bit IAU.MUL, control */
  out[3] = 0xFFFFFFFFu / (a | 1u);  /* constant-division path (BuildUDIV -> mulhu) */
  out[4] = a;
  out[5] = b;
  out[6] = 0;
  out[7] = 0;
}
