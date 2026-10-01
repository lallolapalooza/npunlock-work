/* register-preservation probe: 8 values must survive an expf call */
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>
float expf(float);

void controlled_act(unsigned layerParams) {
  act_abi_invocation invocation;
  ACT_ABI_LOAD_INVOCATION32_OR_RETURN(layerParams, invocation);
  const float *in = ACT_ABI_INPUT_PTR32(const float, invocation, 0u);
  float *out = ACT_ABI_OUTPUT_PTR32(float, invocation, 1u);
  float x = in[0];
  float v0 = x * 3.0f + 1.5f;
  float v1 = x * 5.0f + 2.5f;
  float v2 = x * 7.0f + 3.5f;
  float v3 = x * 9.0f + 4.5f;
  float v4 = x * 11.0f + 5.5f;
  float v5 = x * 13.0f + 6.5f;
  float v6 = x * 15.0f + 7.5f;
  float v7 = x * 17.0f + 8.5f;
  float r = expf(x);
  out[0] = r;
  out[1] = v0; out[2] = v1; out[3] = v2; out[4] = v3;
  out[5] = v4; out[6] = v5; out[7] = v6; out[8] = v7;
}
