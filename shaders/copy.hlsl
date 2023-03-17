[[vk::binding(0)]] cbuffer shaderConsts : register(b0)
{
  float4 sizeAndInvSize; 
}
    
[[vk::binding(1)]] Texture2D<float4>  gSrc    : register(t0);
[[vk::binding(2)]] RWTexture2D<float4> outDst : register(u0);

[shader("compute")]
[numthreads(16, 16, 1)]
void main( uint3 DTid : SV_DispatchThreadID)
{
  uint2 pos = DTid.xy;
  if ( any ( pos >= (uint2)sizeAndInvSize.xy ) )
  {
    return;
  }

  outDst[pos] = gSrc[pos];  
}


