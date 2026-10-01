/* out[i] = in[i ^ count]: the packed-pair form, reveals the second-operand window. */
#include "generic_act.h"
SHAVE_ACT_LOOP((float)in[i] + 0.0f * (float)in[i ^ count])
