/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

//**************************************************
//định nghĩa các hàm dùng cho định dạng nén 3stc_dxt
//**************************************************
#ifndef _3STC_DXT_
#define _3STC_DXT_

#include "HQPrimitiveDataType.h"

void FlipFullBlockDXT1(hq_ubyte8 * pBlock);//đảo 1 block trong định dạng nén DXT1 theo chiều dọc
void FlipFullBlockDXT3(hq_ubyte8 * pBlock);//đảo 1 block trong định dạng nén DXT3 theo chiều dọc
void FlipFullBlockDXT5(hq_ubyte8 * pBlock);//đảo 1 block trong định dạng nén DXT5 theo chiều dọc

void FlipHalfBlockDXT1(hq_ubyte8 * pBlock);//đảo 1 nửa block trong định dạng nén DXT1 theo chiều dọc
void FlipHalfBlockDXT3(hq_ubyte8 * pBlock);//đảo 1 nửa block trong định dạng nén DXT3 theo chiều dọc
void FlipHalfBlockDXT5(hq_ubyte8 * pBlock);//đảo 1 nửa block trong định dạng nén DXT5 theo chiều dọc

void FlipHFullBlockDXT1(hq_ubyte8 * pBlock);//đảo 1 block trong định dạng nén DXT1 theo chiều ngang
void FlipHFullBlockDXT3(hq_ubyte8 * pBlock);//đảo 1 block trong định dạng nén DXT3 theo chiều ngang
void FlipHFullBlockDXT5(hq_ubyte8 * pBlock);//đảo 1 block trong định dạng nén DXT5 theo chiều ngang

void FlipHHalfBlockDXT1(hq_ubyte8 * pBlock);//đảo 1 nửa block trong định dạng nén DXT1 theo chiều ngang
void FlipHHalfBlockDXT3(hq_ubyte8 * pBlock);//đảo 1 nửa block trong định dạng nén DXT3 theo chiều ngang
void FlipHHalfBlockDXT5(hq_ubyte8 * pBlock);//đảo 1 nửa block trong định dạng nén DXT5 theo chiều ngang

void SwapBlockDXT1(hq_ubyte8 *pBlock1,hq_ubyte8 *pBlock2);//hoán đổi 2 block trong dạng nén DXT1
void SwapBlockDXT3_5(hq_ubyte8 *pBlock1,hq_ubyte8 *pBlock2);//hoán đổi 2 block trong dạng nén DXT3/5

void DecodeDXT1(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB = false);//giải mã dạng nén DXT1=>A8R8G8B8 hoặc A8B8G8R8 nếu flipRGB = true
void DecodeDXT3(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB = false);//giải mã dạng nén DXT3=>A8R8G8B8 hoặc A8B8G8R8 nếu flipRGB = true
void DecodeDXT5(const hq_ubyte8*encodeBlocks,hq_ubyte8* decodeData,const hq_uint32 width,const hq_uint32 height , bool flipRGB = false);//giải mã dạng nén DXT5=>A8R8G8B8 hoặc A8B8G8R8 nếu flipRGB = true

#endif
