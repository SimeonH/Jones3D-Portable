// Minimal replacements for ScummVM common definitions used by the ported SMUSH codecs.
#ifndef SMUSH_SMUSHCOMPAT_H
#define SMUSH_SMUSHCOMPAT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t  byte;
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef uint16_t uint16;
typedef uint32_t uint32;

#define READ_LE_UINT16(p) ((uint16)(((const byte*)(p))[0] | (((const byte*)(p))[1] << 8)))
#define READ_LE_UINT32(p) ((uint32)(((const byte*)(p))[0] | (((const byte*)(p))[1] << 8) | (((const byte*)(p))[2] << 16) | ((uint32)((const byte*)(p))[3] << 24)))

#define error(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while ( 0 )

#endif // SMUSH_SMUSHCOMPAT_H
