/* Generic ACT prologue for the ggml-shave engine (big-tile carriers).
 *
 * layerParams points at an array of 0x28-byte tensor records (see
 * npunlock/include/npunlock/npu3720_kernel.h): record 0 is the input tile and
 * record 1 the output tile.  rank/dims describe the *current invocation*, whose
 * element count is the chunk the graph compiler hands to this ACT target.  For
 * the IR-built carriers this is 8192 fp16 elements per invocation, and the
 * record pointers are windowed to that chunk.
 */
#define MAX_INVOCATION_ELEMENTS 16384u

static __attribute__((always_inline)) inline unsigned shave_load_u32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

#define SHAVE_ACT_LOOP(EXPR)                                                     \
    void controlled_act(unsigned layerParams) {                                  \
        const unsigned char *params = (const unsigned char *)layerParams;        \
        unsigned rank = shave_load_u32(params + 0xc);                            \
        const unsigned char *dims = (const unsigned char *)shave_load_u32(params + 0x10); \
        unsigned count = 1, d, i;                                                \
        const __fp16 *in;                                                        \
        __fp16 *out;                                                             \
        if (rank == 0 || rank > 15 || !dims) return;                             \
        for (d = 0; d < rank; ++d) {                                             \
            unsigned dim = shave_load_u32(dims + d * 4);                         \
            if (dim == 0 || dim > MAX_INVOCATION_ELEMENTS / count) return;        \
            count *= dim;                                                        \
        }                                                                        \
        in = (const __fp16 *)shave_load_u32(params);                             \
        out = (__fp16 *)shave_load_u32(params + 0x28);                           \
        for (i = 0; i < count; ++i) out[i] = (__fp16)(EXPR);                     \
    }
