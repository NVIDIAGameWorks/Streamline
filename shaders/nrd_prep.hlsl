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
  uint enableAO;
  uint enableSpecular;
  uint enableDiffuse;
  uint enableWorldMotion;
  uint enableCheckerboard;
  uint cameraMotionIncluded;
  uint relax;
}
    
[[vk::binding(1)]] SamplerState LinearSampler : register(s0);

[[vk::binding(2)]] Texture2D<float>  inDepth                             : register(t0);
[[vk::binding(3)]] Texture2D<float4> inMVec                              : register(t1);

[[vk::binding(4)]] RWTexture2D<float4> outMotionVectors                  : register(u0);
[[vk::binding(5)]] RWTexture2D<float4> outViewZ                          : register(u1);

float3 GetPositionWorld(float3 positionClip, float4x4 mat)
{
  float4 result = mul(mat, float4(positionClip, 1.0));
  return result.xyz / result.w;
}

float GetViewZ(float nonLinearDepth)
{
  float4 screenSpacePos = float4( 0.0, 0.0, nonLinearDepth, 1.0 );
  float4 cameraSpacePos = mul( InvProj, screenSpacePos );
  return cameraSpacePos.z / cameraSpacePos.w;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main( uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixelId = DTid.xy;
    if ( any ( pixelId >= (uint2)sizeAndInvSize.xy ) )
    {
        return;
    }

    float2 uv = float2(pixelId.xy + 0.5) * sizeAndInvSize.zw;
    float nonLinearDepth = inDepth.SampleLevel(LinearSampler, uv, 0).x;

    //recalculate linear depth to ViewZ
    float viewZ = GetViewZ(nonLinearDepth);
    outViewZ[pixelId] = viewZ;
        
    // 3D motion?
    if(enableWorldMotion)
    {
        float4 outMotion = inMVec.SampleLevel(LinearSampler, uv, 0);
        outMotion = float4(-outMotion.xyz, outMotion.w) *  float4( 2.0, -2.0, 1.0, 1.0 );

        if(outMotion.w == 0.0f)
        {
            outMotion = float4( 0.0, 0.0, 0.0, 1.0 );
        }
        else
        {
            float2 uvCurrent = ( pixelId + 0.5 ) * sizeAndInvSize.zw;
            float4 screenSpacePos = float4( uvCurrent.x * 2.0 - 1.0, 1.0 - uvCurrent.y * 2.0, nonLinearDepth, 1.0 );
            float3 positionWorld = GetPositionWorld(screenSpacePos.xyz, ScreenToWorld);
            screenSpacePos.xyz += outMotion.xyz;

            float3 positionWorldPrev = GetPositionWorld(screenSpacePos.xyz, ScreenToWorldPrev);
            outMotion = float4((positionWorldPrev) - (positionWorld), 1.0);
        }

        outMotionVectors[pixelId] = outMotion;
    }  
    else 
    {
        float2 velocityRaw = inMVec.SampleLevel(LinearSampler, uv, 0).xy;
        float2 velocity = -velocityRaw;
        [branch]
        if (!cameraMotionIncluded && any(velocityRaw > 1.0) || any(velocityRaw < -1.0) || (velocityRaw.x == 0.0 && velocityRaw.y == 0.0))
        {
            float2 uvCurrent = ( pixelId + 0.5 ) * sizeAndInvSize.zw;
            float4 screenSpacePosCurrent = float4( uvCurrent.x * 2.0 - 1.0, 1.0 - uvCurrent.y * 2.0, nonLinearDepth, 1.0 );
            float4 screenSpacePosPrevious = mul( CurrentToPreviousClipMatrixNoOffset, screenSpacePosCurrent );
            float2 uvPrevious = float2( 0.5, -0.5 ) * screenSpacePosPrevious.xy / screenSpacePosPrevious.w + 0.5;
            velocity = ( uvPrevious - uvCurrent );
        }

        outMotionVectors[pixelId] = float4( velocity * sizeAndInvSize.xy, 0.0, 0.0 ); //MVs in pixels
    }
}


