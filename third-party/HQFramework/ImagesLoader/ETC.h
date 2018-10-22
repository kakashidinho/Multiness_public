/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef ETC_COMPRESSION_H
#define ETC_COMPRESSION_H
#include "ImgLoader.h"

void DecodeETC1(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB = false);//giải mã dạng nén DXT1=>R8G8B8 hoặc B8G8R8 nếu flipRGB = true
void FlipHFullBlockETC1(hq_ubyte8 * pBlock);
void FlipHHalfBlockETC1(hq_ubyte8 * pBlock);
void FlipFullBlockETC1(hq_ubyte8 * pBlock);
void FlipHalfBlockETC1(hq_ubyte8 * pBlock);
void SwapBlockETC1(hq_ubyte8 *pBlock1,hq_ubyte8 *pBlock2);

#endif
