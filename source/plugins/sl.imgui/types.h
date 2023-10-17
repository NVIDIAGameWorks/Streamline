#pragma once

#include <inttypes.h>
#include <limits.h>

namespace sl
{
namespace type
{

struct Float2
{
    float x, y;
};

struct Float3
{
    float x, y, z;
};

struct Float4
{
    float x, y, z, w;
};

struct Byte2
{
    int8_t x, y;
};

struct Byte3
{
    int8_t x, y, z;
};

struct Byte4
{
    int8_t x, y, z, w;
};

struct UByte2
{
    uint8_t x, y;
};

struct UByte3
{
    uint8_t x, y, z;
};

struct UByte4
{
    uint8_t x, y, z, w;
};

struct Short2
{ 
    int16_t x, y;
};

struct Short3
{
    int16_t x, y, z;
};

struct Short4
{
    int16_t x, y, z, w;
};

struct Int2
{
    int32_t x, y;
};

struct Int3
{
    int32_t x, y, z;
};

struct Int4
{
    int32_t x, y, z, w;
};

struct Uint2
{
    uint32_t x, y;
};

struct Uint3
{
    uint32_t x, y, z;
};

struct Uint4
{
    uint32_t x, y, z, w;
};

}
}
