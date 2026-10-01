#include "shave_act.h"
void controlled_act(unsigned layerParams) {
    const unsigned char *params = (const unsigned char *)layerParams;
    unsigned count = shave_chunk_count(params), i;
    const unsigned *in; unsigned *out;
    if (count == 0) return;
    in = (const unsigned *)shave_load_u32(params);
    out = (unsigned *)shave_load_u32(params + 0x28);
    for (i = 0; i < count / 2; ++i) out[i] = in[i];
}
