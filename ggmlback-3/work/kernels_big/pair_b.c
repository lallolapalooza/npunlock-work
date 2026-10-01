/* out[i] = (float)in[i ^ count]: reveals the window the packed second operand
 * is read from. */
#include "generic_act.h"
SHAVE_ACT_LOOP((__fp16)((float)in[i ^ count]))
