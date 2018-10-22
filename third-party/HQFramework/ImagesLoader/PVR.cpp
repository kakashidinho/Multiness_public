/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include <new>
#include <stdlib.h>
#include "string.h"
#include "Bitmap.h"
#include "ImgByteStream.h"

#define PVR_CUBE_MAP 0x00001000
#define PVR_FLIP_VERTICAL 0x00010000
#define PVR_VOLUME_TEXTURE 0x00004000
#define PVR_MIPMAP 0x00000100
#define PVR_HAS_ALPHA 0x00008000

enum PixelType
{
	MGLPT_RGB_565 = 0x2,
	MGLPT_RGB_888 = 0x4,
	MGLPT_ARGB_8888 = 0x5,
	MGLPT_I_8 = 0x7,
	MGLPT_AI_88 = 0x8,
	MGLPT_PVRTC2 = 0xC,
	MGLPT_PVRTC4 = 0xD,

	// OpenGL version of pixel types
	OGL_RGBA_8888 = 0x12,
	OGL_RGB_565 = 0x13,
	OGL_RGB_888 = 0x15,
	OGL_I_8 = 0x16,
	OGL_AI_88 = 0x17,
	OGL_PVRTC2 = 0x18,
	OGL_PVRTC4 = 0x19,
	OGL_BGRA_8888 = 0x1A,
	OGL_A_8,


	// S3TC Encoding
	D3D_DXT1 = 0x20,
	D3D_DXT3 = 0x22,
	D3D_DXT5 = 0x24,
	
	// Ericsson
	ETC_RGB_4BPP = 0x36,
	
	D3D_A8 = 0x40,
			
	D3D_L8 = 0x43,
	D3D_AL_88 = 0x44 ,
	
	// DX10

	DX10_R8G8B8A8_UNORM = 0x61,

	DX10_A8_UNORM = 0x7B , 


	DX10_BC1_UNORM = 0x80,	

	DX10_BC2_UNORM = 0x82,	

	DX10_BC3_UNORM = 0x84 ,	

	// OpenVG

	/* RGB{A,X} channel ordering */
	ePT_VG_A_8 = 0x9B ,

	MGLPT_NOTYPE = 0xffffffff

};

int Bitmap::LoadPVR()
{
	PVR_Header header;
	stream->GetBytes(&header, sizeof (PVR_Header));

	this->width = header.dwWidth;
	this->height = header.dwHeight;
	this->bits = header.dwBitCount;
	if (header.dwMipMapCount > 0)
	{
		this->complex.dwComplexFlags |= SURFACE_COMPLEX_MIPMAP;
		this->complex.nMipMap = header.dwMipMapCount + 1;
	}

	if (header.dwpfFlags & PVR_CUBE_MAP)//cube map
	{
		if (header.dwNumSurfs != 6)
			return IMG_FAIL_NOT_ENOUGH_CUBE_FACES;
		this->complex.dwComplexFlags |= SURFACE_COMPLEX_CUBE | SURFACE_COMPLEX_FULL_CUBE_FACES;
	}
	else if (header.dwpfFlags & PVR_VOLUME_TEXTURE)//volume texture
	{
		this->complex.dwComplexFlags |= SURFACE_COMPLEX_VOLUME;
		this->complex.vDepth = header.dwNumSurfs;
	}

	//origin
	if (header.dwpfFlags & PVR_FLIP_VERTICAL)
		this->origin = ORIGIN_BOTTOM_LEFT;
	else
		this->origin = ORIGIN_TOP_LEFT;

	//format
	int result = this->CheckPVRFormat(header);
	if (result != IMG_OK)
		return result;


	//load data
	return this->LoadPVRData();
}

int Bitmap::CheckPVRFormat(const PVR_Header &header)
{
	switch (header.dwpfFlags & 0xff)
	{
	case MGLPT_RGB_565: case OGL_RGB_565:
		if (header.dwRBitMask == 0xf800 && header.dwGBitMask == 0x7e0 && header.dwBBitMask == 0x1f)
			this->format = FMT_R5G6B5;
		else
			this->format = FMT_B5G6R5;
		break;
	case MGLPT_RGB_888: case OGL_RGB_888 :
		if (header.dwRBitMask == 0xff0000 && header.dwGBitMask == 0xff00 && header.dwBBitMask == 0xff)
			this->format = FMT_R8G8B8;
		else
			this->format = FMT_B8G8R8;
		break;
	case MGLPT_ARGB_8888: case OGL_BGRA_8888: case OGL_RGBA_8888: 
		if (header.dwRBitMask == 0xff0000 && header.dwGBitMask == 0xff00 && header.dwBBitMask == 0xff)
			this->format = FMT_A8R8G8B8;
		else
			this->format = FMT_A8B8G8R8;
		break;
	case MGLPT_I_8 : case OGL_I_8: case D3D_L8:
		this->format = FMT_L8;
		break;
	case MGLPT_AI_88 : case OGL_AI_88: case D3D_AL_88:
		this->format = FMT_A8L8;
		break;
	case MGLPT_PVRTC2: case OGL_PVRTC2:
		if (header.dwAlphaBitMask == 1)
			this->format = FMT_PVRTC_RGBA_2BPP;
		else
			this->format = FMT_PVRTC_RGB_2BPP;
		break;

	case MGLPT_PVRTC4 : case OGL_PVRTC4:
		if (header.dwAlphaBitMask == 1)
			this->format = FMT_PVRTC_RGBA_4BPP;
		else
			this->format = FMT_PVRTC_RGB_4BPP;
		break;

		// OpenGL version of pixel types
	case DX10_R8G8B8A8_UNORM:
		this->format = FMT_A8B8G8R8;
		break;
	
	case OGL_A_8: case D3D_A8: case DX10_A8_UNORM: case ePT_VG_A_8:
		this->format = FMT_A8;
		break;

	case D3D_DXT1 : case DX10_BC1_UNORM:
		this->format = FMT_S3TC_DXT1;
		break;
	case D3D_DXT3 : case DX10_BC2_UNORM:
		this->format = FMT_S3TC_DXT3;
		break;
	case D3D_DXT5 : case DX10_BC3_UNORM:
		this->format = FMT_S3TC_DXT5;
		break;
		
	case ETC_RGB_4BPP :
		this->format = FMT_ETC1;
		break;
	default:
		return IMG_FAIL_NOT_SUPPORTED;	
	}

	return IMG_OK;
}

int Bitmap::LoadPVRData()
{
	hquint32 levels = ((complex.dwComplexFlags & SURFACE_COMPLEX_MIPMAP) != 0) ? complex.nMipMap : 1;
	hquint32 units = 1;
	if (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE)
		units = 6;
	else if (complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
		units = complex.vDepth;

	//get data
	if (this->complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
	{
		//get size of each mipmap level
		hquint32 *sizeOfLevel = (hquint32 *) malloc(levels * sizeof (hquint32));
		if (sizeOfLevel == NULL)
			return IMG_FAIL_MEM_ALLOC;

		hquint32 w = this->width;
		hquint32 h = this->height;
		for (hquint32 level = 0 ; level < levels ; ++level)
		{
			this->imgSize += (sizeOfLevel[level] = this->CalculateSize(w , h));
			w >>= 1;
			h >>= 1;
			if (w == 0) w = 1;
			if (h == 0) h = 1;
		}

		this->imgSize *= units;

		try{
			
			this->pData = HQ_NEW hqubyte8 [this->imgSize];
		}
		catch (std::bad_alloc e)
		{
			return IMG_FAIL_MEM_ALLOC;
		}

		
		//for each z slice
		for (hquint32 slice = 0; slice < units ; ++slice)
		{
			hqubyte8* pPixel = pData;
			for (hquint32 level = 0 ; level < levels ; ++level)
			{
				pPixel += slice * sizeOfLevel[level];
				this->GetPixelDataFromStream(pPixel, sizeOfLevel[level]);
				pPixel += (units - slice) * sizeOfLevel[level];
			}
		}

		free(sizeOfLevel);
	}
	else//not volume texture
	{
		//get image size
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
			
			this->pData = HQ_NEW hqubyte8 [this->imgSize];
		}
		catch (std::bad_alloc e)
		{
			return IMG_FAIL_MEM_ALLOC;
		}

		this->GetPixelDataFromStream(this->pData, this->imgSize);
	}
	return IMG_OK;
}

void Bitmap::GetPVRHeader(PVR_Header &header) const
{
	header.dwHeaderSize = 52;
	header.dwWidth = width;
	header.dwHeight = height;

	header.dwpfFlags = 0;

	if (origin == ORIGIN_BOTTOM_LEFT)
		header.dwpfFlags |= PVR_FLIP_VERTICAL;

	memcpy(&header.dwPVR, "PVR!", 4);
	if (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE)
	{
		header.dwpfFlags |= PVR_CUBE_MAP;
		header.dwNumSurfs = 6;
		header.dwDataSize = this->imgSize / 6;
	}
	else if (complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
	{
		header.dwpfFlags |= PVR_VOLUME_TEXTURE;
		header.dwNumSurfs = complex.vDepth;
		header.dwDataSize = this->imgSize;
	}
	else
	{
		header.dwNumSurfs = 1;
		header.dwDataSize = this->imgSize;
	}
	if (complex.dwComplexFlags & SURFACE_COMPLEX_MIPMAP)
	{
		header.dwpfFlags |= PVR_MIPMAP;
		header.dwMipMapCount = complex.nMipMap - 1;
	}

	switch (format)
	{
	case FMT_R8G8B8:
		header.dwpfFlags |= MGLPT_RGB_888;
		header.dwBitCount = 24;
		header.dwRBitMask = 0xff0000; header.dwGBitMask = 0xff00; header.dwBBitMask = 0xff; header.dwAlphaBitMask = 0x0000;
		break;
	case FMT_B8G8R8:
		header.dwpfFlags |= OGL_RGB_888;
		header.dwBitCount = 24;
		header.dwBBitMask = 0xff0000; header.dwGBitMask = 0xff00; header.dwRBitMask = 0xff; header.dwAlphaBitMask = 0x0000;
		break;
	case FMT_A8R8G8B8:
		header.dwpfFlags |= MGLPT_ARGB_8888 | PVR_HAS_ALPHA;
		header.dwBitCount = 32;
		header.dwRBitMask = 0xff0000; header.dwGBitMask = 0xff00; header.dwBBitMask = 0xff; header.dwAlphaBitMask = 0xff000000;
		break;
	case FMT_X8R8G8B8:
		header.dwpfFlags |= MGLPT_ARGB_8888;
		header.dwBitCount = 32;
		header.dwRBitMask = 0xff0000; header.dwGBitMask = 0xff00; header.dwBBitMask = 0xff; header.dwAlphaBitMask = 0x000000;
		break;
	case FMT_R5G6B5:
		header.dwpfFlags |= MGLPT_RGB_565;
		header.dwBitCount = 16;
		header.dwRBitMask = 0xf800; header.dwGBitMask = 0x7e0; header.dwBBitMask = 0x1f; header.dwAlphaBitMask = 0x0000;
		break;
	case FMT_B5G6R5:
		header.dwpfFlags |= OGL_RGB_565;
		header.dwBitCount = 16;
		header.dwBBitMask = 0xf800; header.dwGBitMask = 0x7e0; header.dwRBitMask = 0x1f; header.dwAlphaBitMask = 0x0000;
		break;
	case FMT_L8:
		header.dwpfFlags |= MGLPT_I_8;
		header.dwBitCount = 8;
		header.dwRBitMask = 0xff; header.dwGBitMask = 0xff; header.dwBBitMask = 0xff; header.dwAlphaBitMask = 0x00000000;
		break;
	case FMT_A8L8:
		header.dwpfFlags |= MGLPT_AI_88 | PVR_HAS_ALPHA;
		header.dwBitCount = 16;
		header.dwRBitMask = 0xff; header.dwGBitMask = 0xff; header.dwBBitMask = 0xff; header.dwAlphaBitMask = 0xff00;
		break;
	case FMT_S3TC_DXT1:
		header.dwpfFlags |= D3D_DXT1;
		header.dwBitCount = 4;
		header.dwRBitMask = 0x0; header.dwGBitMask = 0x0; header.dwBBitMask = 0x0; header.dwAlphaBitMask = 0x000000;
		break;
	case FMT_S3TC_DXT3:
		header.dwpfFlags |= D3D_DXT3 | PVR_HAS_ALPHA;
		header.dwBitCount = 8;
		header.dwRBitMask = 0x0; header.dwGBitMask = 0x0; header.dwBBitMask = 0x0; header.dwAlphaBitMask = 0x000000;
		break;
	case FMT_S3TC_DXT5:
		header.dwpfFlags |= D3D_DXT5 | PVR_HAS_ALPHA;
		header.dwBitCount = 8;
		header.dwRBitMask = 0x0; header.dwGBitMask = 0x0; header.dwBBitMask = 0x0; header.dwAlphaBitMask = 0x000000;
		break;
	case FMT_A8B8G8R8:
		header.dwpfFlags |= OGL_RGBA_8888 | PVR_HAS_ALPHA;
		header.dwBitCount = 32;
		header.dwRBitMask = 0xff; header.dwGBitMask = 0xff00; header.dwBBitMask = 0xff0000; header.dwAlphaBitMask = 0xff000000;
		break;
	case FMT_X8B8G8R8:
		header.dwpfFlags |= OGL_RGBA_8888;
		header.dwBitCount = 32;
		header.dwRBitMask = 0xff; header.dwGBitMask = 0xff00; header.dwBBitMask = 0xff0000; header.dwAlphaBitMask = 0x000000;
		break;
	case FMT_R8G8B8A8:
		header.dwpfFlags |= OGL_RGBA_8888 | PVR_HAS_ALPHA;
		header.dwBitCount = 32;
		header.dwRBitMask = 0xff000000; header.dwGBitMask = 0xff0000; header.dwBBitMask = 0xff00; header.dwAlphaBitMask = 0xff;
		break;
	case FMT_B8G8R8A8:
		header.dwpfFlags |= OGL_RGBA_8888 | PVR_HAS_ALPHA;
		header.dwBitCount = 32;
		header.dwBBitMask = 0xff000000; header.dwGBitMask = 0xff0000; header.dwRBitMask = 0xff00; header.dwAlphaBitMask = 0xff;
		break;
	case FMT_A8:
		header.dwpfFlags |= OGL_A_8 | PVR_HAS_ALPHA;
		header.dwBitCount = 8;
		header.dwRBitMask = 0x000000; header.dwGBitMask = 0x0000; header.dwBBitMask = 0x00; header.dwAlphaBitMask = 0xff;
		break;
	case FMT_ETC1:
		header.dwpfFlags |= ETC_RGB_4BPP;
		header.dwBitCount = 4;
		header.dwRBitMask = 0x000000; header.dwGBitMask = 0x0000; header.dwBBitMask = 0x00; header.dwAlphaBitMask = 0x00;
		break;
	case FMT_PVRTC_RGB_4BPP:
		header.dwpfFlags |= MGLPT_PVRTC4;
		header.dwBitCount = 4;
		header.dwRBitMask = 0x000000; header.dwGBitMask = 0x0000; header.dwBBitMask = 0x00; header.dwAlphaBitMask = 0x00;
		break;
	case FMT_PVRTC_RGB_2BPP:
		header.dwpfFlags |= MGLPT_PVRTC2;
		header.dwBitCount = 2;
		header.dwRBitMask = 0x000000; header.dwGBitMask = 0x0000; header.dwBBitMask = 0x00; header.dwAlphaBitMask = 0x00;
		break;
	case FMT_PVRTC_RGBA_4BPP:
		header.dwpfFlags |= MGLPT_PVRTC4 | PVR_HAS_ALPHA;
		header.dwBitCount = 4;
		header.dwRBitMask = 0x000000; header.dwGBitMask = 0x0000; header.dwBBitMask = 0x00; header.dwAlphaBitMask = 1;
		break;
	case FMT_PVRTC_RGBA_2BPP:
		header.dwpfFlags |= MGLPT_PVRTC2 | PVR_HAS_ALPHA;
		header.dwBitCount = 2;
		header.dwRBitMask = 0x000000; header.dwGBitMask = 0x0000; header.dwBBitMask = 0x00; header.dwAlphaBitMask = 1;
		break;
	}

}
