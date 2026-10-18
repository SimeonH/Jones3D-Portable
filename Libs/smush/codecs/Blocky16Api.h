// C interface of SMUSH Blocky16 (16-bit RGB565) video codec, ported from ScummVM
#ifndef SMUSH_BLOCKY16API_H
#define SMUSH_BLOCKY16API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* Blocky16_Create(int width, int height);
void Blocky16_Decode(void* pDecoder, uint8_t* pDst, const uint8_t* pSrc);
void Blocky16_Destroy(void* pDecoder);

#ifdef __cplusplus
}
#endif

#endif // SMUSH_BLOCKY16API_H
