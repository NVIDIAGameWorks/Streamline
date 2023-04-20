#define NRD_COMPILER_DXC
#define NRD_USE_OCT_NORMAL_ENCODING 0
#define NRD_USE_MATERIAL_ID 0
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
  uint enableAO;
  uint enableSpecular;
  uint enableDiffuse;
  uint enableWorldMotion;
  uint enableCheckerboard;
  uint cameraMotionIncluded;
  uint relax;
}
    
[[vk::binding(1)]] SamplerState LinearSampler : register(s0);

[[vk::binding(2)]] Texture2D<float>  inViewZ                             : register(t0);
[[vk::binding(3)]] Texture2D<float4> inNormalRoughness                   : register(t1);
[[vk::binding(4)]] Texture2D<float4> inDiffuse                           : register(t2);
[[vk::binding(5)]] Texture2D<float4> inSpecular                          : register(t3);
[[vk::binding(6)]] Texture2D<float4> inAO                                : register(t4);

[[vk::binding(7)]] RWTexture2D<float4> outPackedDiffuse                  : register(u0);
[[vk::binding(8)]] RWTexture2D<float4> outPackedSpecular                 : register(u1);
[[vk::binding(9)]] RWTexture2D<float4> outPackedAO                       : register(u2);

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
        
    // Pack specular signal
    if(enableSpecular)
    {
        float4 specular = inSpecular[pixelId];
        float4 packedRadiance = specular;
        if (!relax)
        {
            float hitT = specular.w;
            float normHitDist = REBLUR_FrontEnd_GetNormHitDist(hitT, viewZ, hitDistParams, normalRoughness.w);
            packedRadiance = REBLUR_FrontEnd_PackRadianceAndHitDist(specular.xyz, normHitDist);
        }
        outPackedSpecular[pixelId] = packedRadiance;
    }

    // Pack AO signal
    if (enableAO) 
    {
    #if 0
        float hitT = inAO[pixelId].r;
        float normHitDist = REBLUR_FrontEnd_GetNormHitDist(hitT, viewZ, hitDistParams);
        outPackedAO[pixelId] = normHitDist;
    #else
        outPackedAO[pixelId] = inAO[pixelId];
    #endif
    }

    // Pack diffuse signal
    if(enableDiffuse)
    {
        float4 diffuse = inDiffuse[pixelId];
        float4 packedRadiance = diffuse;
        if (!relax)
        {
            float hitT = diffuse.w;
            float normHitDist = REBLUR_FrontEnd_GetNormHitDist(hitT, viewZ, hitDistParams, normalRoughness.w);
            packedRadiance = REBLUR_FrontEnd_PackRadianceAndHitDist(diffuse.xyz, normHitDist);
        }
        outPackedDiffuse[pixelId] = packedRadiance;
    }
}


