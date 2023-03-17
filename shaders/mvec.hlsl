[[vk::binding(0)]] Texture2D<float4> texMVec : register(t0);
[[vk::binding(1)]] Texture2D<float4> texDepth : register(t1);
[[vk::binding(2)]] RWTexture2D<float2> rwtexMVec : register(u0);

[[vk::binding(3)]] cbuffer shaderConsts : register(b0)
{
    float4x4 currentToPreviousClipMatrixNoOffset;
    float4 textureSize;
    float2 mvecScale;
    uint showMvec;
};

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixelId = DTid.xy;
    float2 pixelCenter = float2(pixelId)+0.5;

    if (any(pixelId >= (uint2)textureSize.xy))
    {
        return;
    }

    float2 velocity = texMVec[pixelId].xy;

    [branch]
    if (any(velocity > 1.0) || any(velocity < -1.0) || all(velocity == 0.0))
    {
        float2 uvCurrent = pixelCenter * textureSize.zw;
        float nonLinearDepth = texDepth[pixelId].x;
        float4 screenSpacePosCurrent = float4(float2(uvCurrent.x * 2.0 - 1.0, 1.0 - uvCurrent.y * 2.0), nonLinearDepth, 1.0);
        float4 screenSpacePosPrevious = mul(currentToPreviousClipMatrixNoOffset, screenSpacePosCurrent);
        float2 uvPrevious = float2(0.5, -0.5) * screenSpacePosPrevious.xy / screenSpacePosPrevious.w + 0.5;
        velocity = uvCurrent - uvPrevious;
    }
    else
    {
        velocity *= mvecScale; // to -1,1 range
    }
    if (showMvec) velocity *= textureSize.xy * 10.0f;
    rwtexMVec[pixelId] = float2(-velocity) * textureSize.xy; // to pixel space
}
