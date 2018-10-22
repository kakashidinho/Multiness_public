/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include <string.h>
#include <new>
#include "Bitmap.h"
#include "ImgByteStream.h"

#ifndef DDPF_ALPHAPIXELS
#define DDPF_ALPHAPIXELS 0x1
#define DDPF_ALPHA 0x2
#define DDPF_FOURCC 0x4
#define DDPF_RGB 0x40
#define DDPF_YUV 0x200
#define DDPF_LUMINANCE 0x20000
#endif


#ifndef DDSD_CAPS
#define DDSD_CAPS		0x1
#define DDSD_HEIGHT		0x2
#define DDSD_WIDTH		0x4
#define DDSD_PITCH		0x8
#define DDSD_PIXELFORMAT		0x1000
#define DDSD_MIPMAPCOUNT		0x20000
#define DDSD_LINEARSIZE		0x80000
#define DDSD_DEPTH		0x800000
#endif

#ifndef DDSCAPS_COMPLEX
#define DDSCAPS_COMPLEX		0x8
#define DDSCAPS_MIPMAP		0x400000
#define DDSCAPS_TEXTURE		0x1000
#endif

#ifndef DDSCAPS2_CUBEMAP
#define DDSCAPS2_CUBEMAP		0x200
#define DDSCAPS2_CUBEMAP_POSITIVEX		0x400
#define DDSCAPS2_CUBEMAP_NEGATIVEX		0x800
#define DDSCAPS2_CUBEMAP_POSITIVEY		0x1000
#define DDSCAPS2_CUBEMAP_NEGATIVEY		0x2000
#define DDSCAPS2_CUBEMAP_POSITIVEZ		0x4000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ		0x8000
#define DDSCAPS2_VOLUME		0x200000
#endif

#define FOURCC(a, b, c, d) \
((hq_uint32)((hq_uint32)(a) ) | \
((hq_uint32)(b) << 8) | \
((hq_uint32)(c) << 16) | \
((hq_uint32)(d) << 24))

#define FOURCC_DXT1 FOURCC('D','X','T','1')
#define FOURCC_DXT3 FOURCC('D','X','T','3')
#define FOURCC_DXT5 FOURCC('D','X','T','5')


struct DDS_PixelFormat{
	hq_uint32 dwSize;
	hq_uint32 dwFlags;
	hq_uint32 dwFourCC;
	hq_uint32 dwRGBBitCount;
	hq_uint32 dwRBitMask;
	hq_uint32 dwGBitMask;
	hq_uint32 dwBBitMask;
	hq_uint32 dwABitMask;
};


struct DDS_Header{
	hq_uint32 dwSize;
	hq_uint32 dwFlags;
	hq_uint32 dwHeight;
	hq_uint32 dwWidth;
	hq_uint32 dwLinearSize;
	hq_uint32 dwDepth;
	hq_uint32 dwMipMapCount;
	hq_uint32 dwReserved1[11];
	DDS_PixelFormat ddpf;
	hq_uint32 dwCaps;
	hq_uint32 dwCaps2;
	hq_uint32 dwCaps3;
	hq_uint32 dwCaps4;
	hq_uint32 dwReserved2;
};


static SurfaceFormat CheckFormat(const DDS_PixelFormat * pFormatInfo, hqshort16 & bits_per_pixel);
static void CheckComplex(const DDS_Header & header,SurfaceComplexity *pComplex);

int Bitmap::LoadDDS()
{
	DDS_Header  header;
	stream->Seek(4);
	stream->GetBytes(&header, sizeof(DDS_Header));
	if(header.dwSize!=124)
		return IMG_FAIL_BAD_FORMAT;
	if(header.ddpf.dwSize!=32)
		return IMG_FAIL_BAD_FORMAT;

	this->width=header.dwWidth;
	this->height=header.dwHeight;
	
	this->format=CheckFormat(&(header.ddpf), this->bits);//kiểm tra format của dữ liệu ảnh
	
	CheckComplex(header,&complex);//kiểm tra độ phức tạp của dữ liệu ảnh

	if ((this->complex.dwComplexFlags & SURFACE_COMPLEX_CUBE) != 0 && 
		(this->complex.dwComplexFlags & SURFACE_COMPLEX_FULL_CUBE_FACES) != SURFACE_COMPLEX_FULL_CUBE_FACES)//ko đủ 6 mặt
		return IMG_FAIL_NOT_ENOUGH_CUBE_FACES;
	
	this->origin= ORIGIN_TOP_LEFT;
	//get image size
	this->imgSize=0;

	hquint32 levels = ((complex.dwComplexFlags & SURFACE_COMPLEX_MIPMAP) != 0) ? complex.nMipMap : 1;
	hquint32 units = 1;
	if (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE)
		units = 6;
	else if (complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
		units = complex.vDepth;

	hquint32 w = this->width;
	hquint32 h = this->height;
	for (hquint32 level = 0 ; level < levels ; ++level)
	{
		this->imgSize += this->CalculateSize(w , h);
		w >>= 1;
		h >>= 1;
		if (w == 0) w = 1;
		if (h == 0) h = 1;
	}

	this->imgSize *= units;

	try{
		this->pData = HQ_NEW hq_ubyte8[this->imgSize];
	}
	catch(std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}
	
	stream->Seek(128);
	
	this->GetPixelDataFromStream(this->pData, this->imgSize);


	return IMG_OK;
}

SurfaceFormat CheckFormat(const DDS_PixelFormat * pFormatInfo, hqshort16 &bpp)
{
	bpp = pFormatInfo->dwRGBBitCount;

	if(pFormatInfo->dwFlags & DDPF_FOURCC)//dạng nén
	{
		switch(pFormatInfo->dwFourCC)
		{
		case FOURCC_DXT1:
			return FMT_S3TC_DXT1;
		case FOURCC_DXT3:
			return FMT_S3TC_DXT3;
		case FOURCC_DXT5:
			return FMT_S3TC_DXT5;
		case 114:
			bpp = 32;//32 bit float
			return FMT_R32_FLOAT;
		default:
			return FMT_UNKNOWN;
		}
	}

	bool containAlpha=((pFormatInfo->dwFlags & DDPF_ALPHAPIXELS)!=0);//có chứa alpha channel

	if (pFormatInfo->dwFlags & DDPF_RGB)// chứa rgb pixel không nén
	{
		if(containAlpha)
		{
			if(pFormatInfo->dwRGBBitCount==32)
			{
				if(pFormatInfo->dwRBitMask==0xff0000 && pFormatInfo->dwGBitMask==0xff00 &&
				   pFormatInfo->dwBBitMask==0xff && pFormatInfo->dwABitMask==0xff000000)
					return FMT_A8R8G8B8;

				if(pFormatInfo->dwRBitMask==0xff && pFormatInfo->dwGBitMask==0xff00 &&
				   pFormatInfo->dwBBitMask==0xff0000 && pFormatInfo->dwABitMask==0xff000000)
					return FMT_A8B8G8R8;
				return FMT_UNKNOWN;
			}
			return FMT_UNKNOWN;
		}
		if(pFormatInfo->dwRGBBitCount==32)
		{
			if(pFormatInfo->dwRBitMask==0xff0000 && pFormatInfo->dwGBitMask==0xff00 &&
			   pFormatInfo->dwBBitMask==0xff)
				return FMT_X8R8G8B8;

			if(pFormatInfo->dwRBitMask==0xff && pFormatInfo->dwGBitMask==0xff00 &&
			   pFormatInfo->dwBBitMask==0xff0000)
				return FMT_X8B8G8R8;

			if (pFormatInfo->dwRBitMask == 0xffffffff && pFormatInfo->dwGBitMask == 0 &&
				pFormatInfo->dwBBitMask == 0)
				return FMT_R32_FLOAT;
			return FMT_UNKNOWN;
		}
		if(pFormatInfo->dwRGBBitCount==24)
		{
			if(pFormatInfo->dwRBitMask==0xff0000 && pFormatInfo->dwGBitMask==0xff00 &&
			   pFormatInfo->dwBBitMask==0xff)
				return FMT_R8G8B8;

			if(pFormatInfo->dwBBitMask==0xff0000 && pFormatInfo->dwGBitMask==0xff00 &&
			   pFormatInfo->dwRBitMask==0xff)
				return FMT_B8G8R8;

			return FMT_UNKNOWN;
		}
		if(pFormatInfo->dwRGBBitCount==16)
		{
			if(pFormatInfo->dwRBitMask==0xf800 && pFormatInfo->dwGBitMask==0x7e0 &&
			   pFormatInfo->dwBBitMask==0x1f)
				return FMT_R5G6B5;

			if(pFormatInfo->dwBBitMask==0xf800 && pFormatInfo->dwGBitMask==0x7e0 &&
			   pFormatInfo->dwRBitMask==0x1f)
				return FMT_B5G6R5;

			return FMT_UNKNOWN;
		}
		return FMT_UNKNOWN;
	}
	if (pFormatInfo->dwFlags & DDPF_LUMINANCE)//chứa greyscale pixel
	{
		if(containAlpha)
		{
			if(pFormatInfo->dwRGBBitCount==16)
			{
				if(pFormatInfo->dwRBitMask==0xff && pFormatInfo->dwABitMask==0xff00)
					return FMT_A8L8;
				return FMT_UNKNOWN;
			}
			return FMT_UNKNOWN;
		}
		if(pFormatInfo->dwRGBBitCount==8)
		{
			if(pFormatInfo->dwRBitMask==0xff)
				return FMT_L8;
			return FMT_UNKNOWN;
		}
		return FMT_UNKNOWN;
	}
	if(pFormatInfo->dwFlags & DDPF_ALPHA)
	{
		if(pFormatInfo->dwRGBBitCount==8)
		{
			if(pFormatInfo->dwABitMask==0xff)
				return FMT_A8;
			return FMT_UNKNOWN;
		}
	}
	return FMT_UNKNOWN;
}

void CheckComplex(const DDS_Header & header,SurfaceComplexity *pComplex)
{
	if(header.dwCaps & DDSCAPS_MIPMAP)//có mipmap
	{
		pComplex->dwComplexFlags |= SURFACE_COMPLEX_MIPMAP;
		pComplex->nMipMap = header.dwMipMapCount;//số lượng mipmap level
	}
	if(header.dwCaps2 & DDSCAPS2_CUBEMAP)//có chứa các mặt của cube texture
	{
		pComplex->dwComplexFlags |=SURFACE_COMPLEX_CUBE;
		if(header.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEX)//chứa mặt phía Ox+
			pComplex->dwComplexFlags |= SURFACE_COMPLEX_CUBE_POS_X;
		if(header.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX)//chứa mặt phía Ox-
			pComplex->dwComplexFlags |= SURFACE_COMPLEX_CUBE_NEG_X;
		if(header.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEY)//chứa mặt phía Oy+
			pComplex->dwComplexFlags |= SURFACE_COMPLEX_CUBE_POS_Y;
		if(header.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY)//chứa mặt phía Oy+
			pComplex->dwComplexFlags |= SURFACE_COMPLEX_CUBE_NEG_Y;
		if(header.dwCaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ)//chứa mặt phía Oz+
			pComplex->dwComplexFlags |= SURFACE_COMPLEX_CUBE_POS_Z;
		if(header.dwCaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ)//chứa mặt phía Oz-
			pComplex->dwComplexFlags |= SURFACE_COMPLEX_CUBE_NEG_Z;
	}
	else if (header.dwCaps2 & DDSCAPS2_VOLUME)//chứa dữ liệu của volume texture
		pComplex->dwComplexFlags |= SURFACE_COMPLEX_VOLUME;
}
