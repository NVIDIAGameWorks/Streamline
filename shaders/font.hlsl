[[vk::binding(0)]] RWByteAddressBuffer rwText : register(u0);
[[vk::binding(1)]] RWByteAddressBuffer rwFont : register(u1);
[[vk::binding(2)]] RWTexture2D<float4> rwOut : register(u2);

[[vk::binding(3)]] cbuffer CB : register(b0)
{   
    float4 color;
    uint text_offset;
    uint x;
    uint y;
    uint width;
    uint height;
    uint surf_offset_x;
    uint surf_offset_y;
    int  reverse_x;
    int  reverse_y;
};

#define FONT_KERNEL_BLOCK_DIM_X      8
#define FONT_KERNEL_BLOCK_DIM_Y      13

uint getByte(RWByteAddressBuffer buffer, uint offset)
{
    uint a = offset % 4;
    return (buffer.Load(offset) >> (8 * a)) & 0xff;
}

// IMPORTANT: PRECOMPILED SO ANY MODIFICATIONS HERE WON'T APPLY UNLESS HEADERS IN 'ROOT/SHADERS' ARE CHANGED

[shader("compute")]
[numthreads(FONT_KERNEL_BLOCK_DIM_X, FONT_KERNEL_BLOCK_DIM_Y, 1)]
void main(uint3 blockIdx : SV_GroupID, uint3 threadIdx : SV_GroupThreadID)
{    
    uint ch = getByte(rwText, text_offset + blockIdx.x);
    uint o = blockIdx.x * (FONT_KERNEL_BLOCK_DIM_X + 1) + threadIdx.x;

    uint b = 0;
    if (ch >= 32 && ch < 127) 
    {
        b = getByte(rwFont, (FONT_KERNEL_BLOCK_DIM_Y * (ch - 32) + (FONT_KERNEL_BLOCK_DIM_Y - 1 - threadIdx.y)));
        b = (b >> (FONT_KERNEL_BLOCK_DIM_X - 1 - threadIdx.x)) & 0x01;
    }

    uint dest_x = x + o;
    dest_x = reverse_x ? width - 1 - dest_x : dest_x;
    uint dest_y = y + threadIdx.y;
    dest_y = reverse_y ? height - 1 - dest_y : dest_y;
    uint2 xy = int2(dest_x + surf_offset_x, dest_y + surf_offset_y);
    rwOut[xy] = color * b;
    if (threadIdx.x == 0) rwOut[xy - uint2(1,0)] = 0;
}
