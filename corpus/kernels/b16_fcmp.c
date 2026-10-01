/* corpus synthetic kernel: floating-point compares and branches */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const float *in = (const float *)load32((const unsigned char *)layerParams);
  float *out = (float *)load32((const unsigned char *)layerParams + 0x28);
  float acc = 0.0f;
  for (unsigned i = 0; i < 16; ++i) {
    float x = in[i];
    if (x > 0.5f) acc += x;
    else if (x < -0.5f) acc -= x;
    if (x == 0.0f) acc += 1.0f;
    acc = (x >= acc) ? x : acc;
  }
  out[0] = acc;
}
