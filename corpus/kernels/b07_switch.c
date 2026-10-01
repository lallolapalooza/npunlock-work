/* corpus synthetic kernel: switch with dense and sparse cases plus fallthrough */
static unsigned load32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void controlled_act(unsigned layerParams) {
  const unsigned *in = (const unsigned *)load32((const unsigned char *)layerParams);
  unsigned *out = (unsigned *)load32((const unsigned char *)layerParams + 0x28);
  unsigned acc = 0;
  for (unsigned i = 0; i < 16; ++i) {
    switch (in[i] & 0xfu) {
      case 0: acc += 1u; break;
      case 1: acc += 2u; /* fallthrough */
      case 2: acc += 3u; break;
      case 3: acc += 5u; break;
      case 4: acc += 7u; break;
      case 5: acc += 11u; break;
      case 6: acc += 13u; break;
      case 7: acc += 17u; break;
      case 8: acc += 19u; break;
      case 9: acc += 23u; break;
      case 10: acc += 29u; break;
      case 11: acc += 31u; break;
      case 12: acc += 37u; break;
      case 1000: acc += 41u; break;
      default: acc += 43u; break;
    }
  }
  out[0] = acc;
}
