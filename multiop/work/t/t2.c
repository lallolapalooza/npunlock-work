static __attribute__((always_inline)) inline unsigned ld32(const unsigned char *p) {
  return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
void controlled_act(unsigned layerParams) {
  const unsigned char *params = (const unsigned char *)layerParams;
  unsigned rank = ld32(params + 0xc);
  const unsigned char *dims = (const unsigned char *)ld32(params + 0x10);
  if (rank == 0u || rank > 15u || dims == 0) return;
  unsigned count = 1u;
  for (unsigned d = 0u; d < rank; ++d) {
    unsigned dim = ld32(dims + d * 4u);
    if (dim == 0u || dim > 16u) return;
    count *= dim;
  }
  const __fp16 *in = (const __fp16 *)ld32(params);
  __fp16 *out = (__fp16 *)ld32(params + 0x28u);
  for (unsigned i = 0u; i < count; ++i) out[i] = in[i];
}
