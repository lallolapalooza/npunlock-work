/* out[i] = (float)in[0]: reveals which input element each invocation's window
 * starts at.  Routed through float so the store is a converting store (a raw
 * fp16 move is discarded by the SHAVE write-back rules). */
#include "generic_act.h"
SHAVE_ACT_LOOP((__fp16)((float)in[0]))
