#define NRD_COMPILER_DXC
#define NRD_USE_OCT_NORMAL_ENCODING 0
#define NRD_USE_MATERIAL_ID 0
#include "../external/nrd/Shaders/Include/NRDEncoding.hlsli"
#include "../external/nrd/Shaders/Include/NRD.hlsli"

[[vk::binding(0)]] cbuffer shaderConsts : register(b0)
{
  float4x4 CurrentToPreviousClipMatrixNoOffset;
  float4x4 InvProj;
  float4x4 ScreenToWorld;
  float4x4 ScreenToWorldPrev;
  float4 sizeAndInvSize;
  float4 hitDistParams;
  uint frameId;
  uint enableWorldMotion;
  uint enableCheckerboard;
  uint cameraMotionIncluded;
  uint relax;
  uint encodeDiffuseRadianceHitDist;
  uint encodeSpecularRadianceHitDist;
  uint encodeDiffuseDirectionHitDist;
  uint encodeDiffuseSh0;
  uint encodeDiffuseSh1;
  uint encodeSpecularSh0;
  uint encodeSpecularSh1;
  uint encodeShadowdata;
  uint encodeShadowTransluscency;
}
    
[[vk::binding(1)]] SamplerState LinearSampler : register(s0);

[[vk::binding(2)]] Texture2D<float>  inViewZ                             : register(t0);
[[vk::binding(3)]] Texture2D<float4> inNormalRoughness                   : register(t1);

[[vk::binding(4)]] Texture2D<float4> inDiffuseRadianceHitDist                           : register(t2);
[[vk::binding(5)]] Texture2D<float4> inSpecularRadianceHitDist                          : register(t3);
[[vk::binding(6)]] Texture2D<uint> inDiffuseDirectionHitDist                                : register(t4);
[[vk::binding(7)]] Texture2D<float4> inDiffuseSh0                           : register(t5);
[[vk::binding(8)]] Texture2D<float4> inDiffuseSh1                          : register(t6);
[[vk::binding(9)]] Texture2D<float4> inSpecularSh0                                : register(t7);
[[vk::binding(10)]] Texture2D<float4> inSpecularSh1                           : register(t8);
[[vk::binding(11)]] Texture2D<float4> inShadowdata                          : register(t9);
[[vk::binding(12)]] Texture2D<uint> inShadowTransluscency                                : register(t10);

[[vk::binding(13)]] RWTexture2D<float4> outDiffuseRadianceHitDist                  : register(u0);
[[vk::binding(14)]] RWTexture2D<float4> outSpecularRadianceHitDist                 : register(u1);
[[vk::binding(15)]] RWTexture2D<uint> outDiffuseDirectionHitDist                       : register(u2);
[[vk::binding(16)]] RWTexture2D<float4> outDiffuseSh0                       : register(u3);
[[vk::binding(17)]] RWTexture2D<float4> outDiffuseSh1                       : register(u4);
[[vk::binding(18)]] RWTexture2D<float4> outSpecularSh0                       : register(u5);
[[vk::binding(19)]] RWTexture2D<float4> outSpecularSh1                       : register(u6);
[[vk::binding(20)]] RWTexture2D<float4> outShadowdata                       : register(u7);
[[vk::binding(21)]] RWTexture2D<uint> outShadowTransluscency                       : register(u8);

[shader("compute")]
[numthreads(16, 16, 1)]
void main( uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixelId = DTid.xy;
    if ( any ( pixelId >= (uint2)sizeAndInvSize.xy ) )
    {
        return;
    }

    uint2 sourcePixelId = pixelId;
    if (enableCheckerboard)
    {
        if (pixelId.x > uint(sizeAndInvSize.x / 2))
            return;
        uint offset = ((sourcePixelId.y ^ 0x1) ^ frameId) & 0x1;
        sourcePixelId.x = (sourcePixelId.x << 1) + offset;
    }

    float viewZ = inViewZ[sourcePixelId];
    float4 normalRoughness = inNormalRoughness[sourcePixelId].w;

    // Pack diffuse signal
    if (encodeDiffuseRadianceHitDist)
    {
        float4 diffuse = inDiffuseRadianceHitDist[pixelId];
        float4 packedRadiance = diffuse;
        if (!relax)
        {
            float hitT = diffuse.w;
            float normHitDist = REBLUR_FrontEnd_GetNormHitDist(hitT, viewZ, hitDistParams, normalRoughness.w);
            packedRadiance = REBLUR_FrontEnd_PackRadianceAndNormHitDist(diffuse.xyz, normHitDist);
        }
        outDiffuseRadianceHitDist[pixelId] = packedRadiance;
    }
        
    // Pack specular signal
    if(encodeSpecularRadianceHitDist)
    {
        float4 specular = inSpecularRadianceHitDist[pixelId];
        float4 packedRadiance = specular;
        if (!relax)
        {
            float hitT = specular.w;
            float normHitDist = REBLUR_FrontEnd_GetNormHitDist(hitT, viewZ, hitDistParams, normalRoughness.w);
            packedRadiance = REBLUR_FrontEnd_PackRadianceAndNormHitDist(specular.xyz, normHitDist);
        }
        outSpecularRadianceHitDist[pixelId] = packedRadiance;
    }
}


