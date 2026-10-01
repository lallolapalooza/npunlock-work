/* Common prologue shared by every ggml-shave ACT kernel.
 *
 * The ACT ABI is the observed NPU3720 contract (see
 * npunlock/include/npunlock/npu3720_kernel.h): layerParams points at an array
 * of 0x28-byte tensor records -- record 0 the first input tile, further records
 * the remaining inputs, then the output tile -- followed by the rank
 * (params + 0x0c) and a pointer to the rank u32 dimensions (params + 0x10).
 * rank/dims describe *this invocation*, whose element count is the chunk the
 * graph compiler hands to the ACT target, and the record pointers are already
 * windowed to it (measured: for the IR-built carriers the invocations tile the
 * tensor in order, so concatenating them covers every byte exactly once).
 *
 * The carriers below are declared fp16 by the graph compiler even though the
 * records they expose are byte windows; the arithmetic kernels therefore
 * reinterpret the record pointers as the dtype the ggml tensor actually holds
 * (see kernels/*.c).  A "element count" of n fp16 slots is n/2 fp32 values.
 *
 * Every kernel must keep its stores inside this loop: a raw fp16 move is
 * compiled to an LSU0 store that the SHAVE write-back rules discard, so all
 * data movement is routed through the f32 register file.
 */
#ifndef GGML_SHAVE_ACT_H
#define GGML_SHAVE_ACT_H

/* Largest chunk a carrier is allowed to hand a single ACT invocation.  The
 * 65536-element carriers used by the backend expose 32768-element chunks. */
#define SHAVE_MAX_INVOCATION 65536u

static __attribute__((always_inline)) inline unsigned shave_load_u32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

/* Resolves rank/dims into the invocation's element count; 0 means "malformed
 * contract, do nothing". */
static __attribute__((always_inline)) inline unsigned shave_chunk_count(const unsigned char *params) {
    unsigned rank = shave_load_u32(params + 0xc);
    const unsigned char *dims = (const unsigned char *)shave_load_u32(params + 0x10);
    unsigned count = 1, d;
    if (rank == 0 || rank > 15 || !dims) {
        return 0;
    }
    for (d = 0; d < rank; ++d) {
        unsigned dim = shave_load_u32(dims + d * 4);
        if (dim == 0 || dim > SHAVE_MAX_INVOCATION / count) {
            return 0;
        }
        count *= dim;
    }
    return count;
}

#define SHAVE_RECORD(params, index) ((const unsigned char *)shave_load_u32((params) + 0x28 * (index)))

/* Unary elementwise kernel over `count` elements of TYPE (all of them). */
#define SHAVE_ACT_UNARY(TYPE, EXPR)                                              \
    void controlled_act(unsigned layerParams) {                                  \
        const unsigned char *params = (const unsigned char *)layerParams;        \
        const TYPE *in;                                                          \
        TYPE *out;                                                               \
        unsigned count = shave_chunk_count(params), i;                           \
        if (count == 0) return;                                                  \
        in = (const TYPE *)SHAVE_RECORD(params, 0);                              \
        out = (TYPE *)SHAVE_RECORD(params, 1);                                   \
        for (i = 0; i < count; ++i) {                                            \
            out[i] = (TYPE)(EXPR);                                               \
        }                                                                        \
    }

/* Binary elementwise kernel over the invocation's elements. */
#define SHAVE_ACT_BINARY(TYPE, EXPR)                                             \
    void controlled_act(unsigned layerParams) {                                  \
        const unsigned char *params = (const unsigned char *)layerParams;        \
        const TYPE *a;                                                           \
        const TYPE *b;                                                           \
        TYPE *out;                                                               \
        unsigned count = shave_chunk_count(params), i;                           \
        if (count == 0) return;                                                  \
        a = (const TYPE *)SHAVE_RECORD(params, 0);                               \
        b = (const TYPE *)SHAVE_RECORD(params, 1);                               \
        out = (TYPE *)SHAVE_RECORD(params, 2);                                   \
        for (i = 0; i < count; ++i) {                                            \
            out[i] = (TYPE)(EXPR);                                               \
        }                                                                        \
    }

#endif /* GGML_SHAVE_ACT_H */
