// stdafx.h : includefile for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <xmmintrin.h>
#endif

#pragma warning (disable: 4244)

// TODO: reference additional headers your program requires here
typedef unsigned char byte;
typedef unsigned char uint8;
typedef unsigned int uint32;
#ifdef _MSC_VER
typedef unsigned __int64 uint64;
typedef signed __int64 int64;
#else
typedef unsigned long long uint64;
typedef signed long long int64;
#endif
typedef signed int int32;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned int uint;

#define assert(x)	\
	if (!(x))		\
	{				\
		appError("assertion failed: %s\n", #x); \
	}

void appError(const char *fmt, ...);

inline uint16 _byteswap_ushort(uint16 i)
{
    uint16 j;
    j =  (i << 8);
    j += (i >> 8);
    return j;
}

inline uint32 _byteswap_ulong(uint32 i)
{
    uint32 j;
    j =  (i << 24);
    j += (i <<  8) & 0x00FF0000;
    j += (i >>  8) & 0x0000FF00;
    j += (i >> 24);
    return j;
}

inline uint64 _byteswap_uint64(uint64 i)
{
    uint64 j;
    j =  (i << 56);
    j += (i << 40) & 0x00FF000000000000;
    j += (i << 24) & 0x0000FF0000000000;
    j += (i <<  8) & 0x000000FF00000000;
    j += (i >>  8) & 0x00000000FF000000;
    j += (i >> 24) & 0x0000000000FF0000;
    j += (i >> 40) & 0x000000000000FF00;
    j += (i >> 56);
    return j;
}

#ifndef _MSC_VER

// GCC __forceinline macro
#define __forceinline inline __attribute__((always_inline))

__forceinline unsigned char _BitScanReverse(unsigned long * const Index, const unsigned long Mask)
{
    *Index = 31 - __builtin_clz(Mask);
	return Mask ? 1 : 0;
}

__forceinline unsigned char _BitScanForward(unsigned long * const Index, const unsigned long Mask)
{
	*Index = __builtin_ctz(Mask);
	return Mask ? 1 : 0;
}

__forceinline uint32 _rotl(uint32 value, int shift)
{
	return (((value) << ((int)(shift))) | ((value) >> (32 - (int)(shift))));
}

#endif // _MSC_VER
