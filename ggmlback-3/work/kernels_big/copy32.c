#include "shave_act.h"
void controlled_act(unsigned layerParams) {
    const unsigned char *params = (const unsigned char *)layerParams;
    unsigned count = shave_chunk_count(params), i;
    const float *in; float *out;
    if (count == 0) return;
    in = (const float *)shave_load_u32(params);
    out = (float *)shave_load_u32(params + 0x28);
    for (i = 0; i < count / 2; ++i) out[i] = in[i];
}
