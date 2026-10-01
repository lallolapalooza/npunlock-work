/* out[i] = in[i] (identity), routed through float so the compiler selects the
 * converting load/store pair. */
#include "generic_act.h"
SHAVE_ACT_LOOP((__fp16)((float)in[i]))
