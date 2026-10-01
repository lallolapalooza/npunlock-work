/* Binary fp32 add through a carrier declared as fp16: the ACT record pointers
 * are byte-windowed, so a tensor of count fp16 slots holds count/2 fp32 values. */
#include "generic_act.h"

void controlled_act(unsigned layerParams) {
    const unsigned char *params = (const unsigned char *)layerParams;
    unsigned rank = shave_load_u32(params + 0xc);
    const unsigned char *dims = (const unsigned char *)shave_load_u32(params + 0x10);
    unsigned count = 1, d;
    const float *a, *b;
    float *o;
    unsigned i;
    if (rank == 0 || rank > 15 || !dims) return;
    for (d = 0; d < rank; ++d) {
        unsigned dim = shave_load_u32(dims + d * 4);
        if (dim == 0 || dim > MAX_INVOCATION_ELEMENTS / count) return;
        count *= dim;
    }
    a = (const float *)shave_load_u32(params + 0x00);
    b = (const float *)shave_load_u32(params + 0x28);
    o = (float *)shave_load_u32(params + 0x50);
    for (i = 0; i < count / 2; ++i) o[i] = a[i] + b[i];
}
