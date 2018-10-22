/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "Bitmap.h"
#include "ImgByteStream.h"
#include <string.h>
#include <new>

struct MaskInfo
{
	hquint32 mask;
	hquint32 shift;
};

inline hquint32 GetChannel(hquint32 pixel, const MaskInfo &maskInfo)
{
	return (pixel & maskInfo.mask) >> maskInfo.shift;
}
//get shift amount
inline void GetShiftAmount(MaskInfo &maskInfo)
{
	maskInfo.shift = 0;
	hquint32 temp = maskInfo.mask;
	while ((temp & 0x1) == 0 && maskInfo.shift < 31)
	{
		temp >>= 1;
		maskInfo.shift ++;
	}
}

int Bitmap::LoadBMP()
{
	

	hq_uint32 offset;//độ dời đến vị trí bắt đầu của pixel data
	stream->Seek(0xA);
	stream->GetBytes(&offset, 4);

	hq_uint32 infoSize;//độ lớn info header
	stream->GetBytes(&infoSize, 4);

#if BMP_HEADER_MUST_BE_40
	if(infoSize!=40)//should be 40
		return IMG_FAIL_BAD_FORMAT;
#endif

	hq_uint32 w,h;
	
	stream->GetBytes(&w, 4);
	stream->GetBytes(&h, 4);
	if(w<1||h<1)
		return IMG_FAIL_BAD_FORMAT;
	this->width=w;
	this->height=h;

	stream->Advance(0x1c - 0x1a);
	stream->GetBytes(&this->bits, 2);
	
	if(this->bits<8)
		return IMG_FAIL_NOT_SUPPORTED;

	hq_ushort16 pixelSize=(this->bits)/8;
	hq_uint32 lineSize=pixelSize*w;
	this->imgSize=lineSize*h;

	this->origin=ORIGIN_BOTTOM_LEFT;

	hq_uint32 encode;//dạng mã hóa
	stream->GetBytes(&encode, 4);


	hq_short16 remain=((pixelSize*w)%4);
	hq_ushort16 linePadding=0;
	if(remain)
		linePadding=4-remain;//độ dài phần byte dư của 1 hàng
			
	switch (encode)//dạng mã hóa
	{
	case 0://raw data ,không nén
		stream->Seek(offset);//vị trí bắt đầu pixel data
		return loadBMPRawData(linePadding,lineSize);
	case 3://bitmask
		return loadBMPBitMaskData(linePadding,lineSize, offset);
		
	default:
		return IMG_FAIL_NOT_SUPPORTED;
	}
}
int Bitmap::loadBMPRawData(hq_ushort16 linePadding,
				hq_uint32 lineSize)
{
	bool flipRGB = false;
	switch (this->bits)
	{
	case 32:
		if (this->layout == LAYOUT_BGR)
		{
			format = FMT_A8B8G8R8;
			flipRGB = true;
		}
		else
			format=FMT_A8R8G8B8;
		break;
	case 24:
		if (this->layout == LAYOUT_BGR)
		{
			format = FMT_B8G8R8;
			flipRGB = true;
		}
		else
			format=FMT_R8G8B8;
		break;
	case 8:
		format=FMT_L8;
		break;
	case 16:
		if (this->layout16 == LAYOUT_BGR)
		{
			format = FMT_B5G6R5;
			flipRGB = true;
		}
		else
			format=FMT_R5G6B5;
		break;
	default:
		format=FMT_UNKNOWN;
		return IMG_FAIL_NOT_SUPPORTED;
	}
	
	try{
		this->pData=new hq_ubyte8[this->imgSize];
	}
	catch(std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}

	hqubyte8 *pCur = this->pData;

	for (hquint32 i = 0 ; i < height; ++i)
	{

		if (flipRGB)
		{
			switch(this->bits)
			{
			case 16:
				{
					hqushort16 pixel;
					for (hquint32 j = 0 ; j < width ; ++j)
					{
						stream->GetBytes(&pixel, 2);
						*((hqushort16*)pCur) = SwapRGB16(pixel);
						pCur += 2;
					}
				}
				break;
			case 24 :
				{
					hqubyte8 pixel[3];
					for (hquint32 j = 0 ; j < width ; ++j)
					{
						stream->GetBytes(pixel, 3);
						pCur[0] = pixel[2];
						pCur[1] = pixel[1];
						pCur[2] = pixel[0];
						pCur += 3;
					}
				}
				break;
			case 32 :
				{
					hqubyte8 pixel[4];
					for (hquint32 j = 0 ; j < width ; ++j)
					{
						stream->GetBytes(pixel, 4);
						pCur[0] = pixel[2];
						pCur[1] = pixel[1];
						pCur[2] = pixel[0];
						pCur[3] = pixel[3];
						pCur += 4;
					}
				}
				break;
			}
		}
		else
		{
			stream->GetBytes(pCur, lineSize);
			pCur+=lineSize;
		}
		stream->Advance(linePadding);
	}

	return IMG_OK;
}

int Bitmap::loadBMPBitMaskData(hq_ushort16 linePadding,
				hq_uint32 lineSize, hquint32 imgDataOffset)
{
	MaskInfo bitmask[4];//red, green, blue, alpha masks
	stream->Seek(0x36);
	for (int i = 0 ;i < 4; ++i)
	{
		stream->GetBytes(&bitmask[i].mask, 4);
		GetShiftAmount(bitmask[i]);
	}
	
	bool flipRGB = false;
	switch (this->bits)
	{
	case 32:
		if (this->layout == LAYOUT_BGR)
		{
			format = FMT_A8B8G8R8;
			flipRGB = true;
		}
		else
			format=FMT_A8R8G8B8;
		break;
	case 16:
		if (this->layout16 == LAYOUT_BGR)
		{
			format = FMT_B5G6R5;
			flipRGB = true;
		}
		else
			format=FMT_R5G6B5;
		break;
	default:
		format=FMT_UNKNOWN;
		return IMG_FAIL_NOT_SUPPORTED;
	}

	try{
		this->pData=new hq_ubyte8[this->imgSize];
	}
	catch(std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}

	stream->Seek(imgDataOffset);//vị trí bắt đầu pixel data

	hqubyte8 *pCur = this->pData;

	for (hquint32 i = 0 ; i < height; ++i)
	{

		if (flipRGB)
		{
			switch(this->bits)
			{
			case 16:
				{
					hqushort16 pixel;
					for (hquint32 j = 0 ; j < width ; ++j)
					{
						stream->GetBytes(&pixel, 2);
						*((hqushort16*)pCur) = RGB16( GetChannel(pixel, bitmask[2]) , 
													  GetChannel(pixel, bitmask[1]) ,
													  GetChannel(pixel, bitmask[0])
													);
						pCur += 2;
					}
				}
				break;
			case 32 :
				{
					hquint32 pixel;
					for (hquint32 j = 0 ; j < width ; ++j)
					{
						stream->GetBytes(&pixel, 4);
						pCur[0] = GetChannel(pixel, bitmask[0]);
						pCur[1] = GetChannel(pixel, bitmask[1]);
						pCur[2] = GetChannel(pixel, bitmask[2]);
						pCur[3] = GetChannel(pixel, bitmask[3]);//alpha
						pCur += 4;
					}
				}
				break;
			}
		}
		else
		{
			switch(this->bits)
			{
			case 16:
				{
					hqushort16 pixel;
					for (hquint32 j = 0 ; j < width ; ++j)
					{
						stream->GetBytes(&pixel, 2);
						*((hqushort16*)pCur) = RGB16( GetChannel(pixel, bitmask[0]) , 
													  GetChannel(pixel, bitmask[1]) ,
													  GetChannel(pixel, bitmask[2])
													);
						pCur += 2;
					}
				}
				break;
			case 32 :
				{
					hquint32 pixel;
					for (hquint32 j = 0 ; j < width ; ++j)
					{
						stream->GetBytes(&pixel, 4);
						pCur[0] = GetChannel(pixel, bitmask[2]);
						pCur[1] = GetChannel(pixel, bitmask[1]);
						pCur[2] = GetChannel(pixel, bitmask[0]);
						pCur[3] = GetChannel(pixel, bitmask[3]);//alpha
						pCur += 4;
					}
				}
				break;
			}
		}
		stream->Advance(linePadding);
	}

	return IMG_OK;
}

