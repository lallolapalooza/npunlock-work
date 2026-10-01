/* 8-entry aligned ACT kernel: entry eN computes in + N. */
#include <npunlock/npu3720_kernel.h>
static __attribute__((always_inline)) inline unsigned m32(const unsigned char *p) { return (unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24); }
#define PRO   const unsigned char *params=(const unsigned char*)layerParams;   unsigned rank=m32(params+0xcu); const unsigned char *dims=(const unsigned char*)m32(params+0x10u);   if(rank==0u||rank>15u||dims==0) return;   unsigned count=1u;   for(unsigned d=0u;d<rank;++d){unsigned dim=m32(dims+d*4u); if(dim==0u||dim>16u/count) return; count*=dim;}   const __fp16 *in=(const __fp16*)m32(params); __fp16 *out=(__fp16*)m32(params+0x28u);   for(unsigned i=0u;i<count;++i)
void entry0(unsigned layerParams) { PRO out[i] = in[i] + 1.0f; }
void entry1(unsigned layerParams) { PRO out[i] = in[i] + 1.0f + 1.0f; }
void entry2(unsigned layerParams) { PRO out[i] = in[i] + 1.0f + 1.0f + 1.0f; }
void entry3(unsigned layerParams) { PRO out[i] = in[i] + 1.0f + 1.0f + 1.0f + 1.0f; }
void entry4(unsigned layerParams) { PRO out[i] = in[i] + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f; }
void entry5(unsigned layerParams) { PRO out[i] = in[i] + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f; }
void entry6(unsigned layerParams) { PRO out[i] = in[i] + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f; }
void entry7(unsigned layerParams) { PRO out[i] = in[i] + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f + 1.0f; }
