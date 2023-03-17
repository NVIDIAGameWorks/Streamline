[[vk::binding(0)]] cbuffer shaderConsts : register(b0)
{
  float4 sizeAndInvSize; 
}
    
[[vk::binding(1)]] Texture2D<float4>  gSrc    : register(t0);
[[vk::binding(2)]] RWByteAddressBuffer outDst : register(u0); // RGBA32F buffer

// IMPORTANT: PRECOMPILED SO ANY MODIFICATIONS HERE WON'T APPLY UNLESS HEADERS IN 'ROOT/SHADERS' ARE CHANGED

[shader("compute")]
[numthreads(16, 16, 1)]
void main( uint3 DTid : SV_DispatchThreadID)
{
  uint2 pixelId = DTid.xy;
  if ( any ( pixelId >= (uint2)sizeAndInvSize.xy ) )
  {
    return;
  }

  // RGB32F buffer
  uint byteOffset = (pixelId.x + pixelId.y * sizeAndInvSize.x) * 12;
  float4 c = gSrc[pixelId];

  outDst.Store(byteOffset, asuint(c.x));
  outDst.Store(byteOffset + 4, asuint(c.y));
  outDst.Store(byteOffset + 8, asuint(c.z));
  //outDst.Store(byteOffset + 12, asuint(c.w));
}


