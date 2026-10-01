/* out[i] = in[i] + in[i ^ count]: the host packs [a | b] into one tile, so
 * lane i of window 0 pairs with lane i + count (operand b) and lane i of
 * window 1 pairs with lane i - count (operand a).  Requires count to be a
 * power of two, which every carrier tile is. */
#include "generic_act.h"
SHAVE_ACT_LOOP((float)in[i] + (float)in[i ^ count])
