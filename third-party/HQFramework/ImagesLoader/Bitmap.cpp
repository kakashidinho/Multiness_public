/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "3STC_DXT.h"
#include "ETC.h"
#include "Bitmap.h"
#include "ImgByteStream.h"
#include <stdio.h>//FILE I/O
#include <string.h>//memcpy,memset
#include <math.h>//floor
#include <new>//bad_alloc

#pragma message ("warning: beware of pData deletion and change when assign it to outside pointer by Wrap method, thus the Bitmap object is not the owner of pData pointer")

#if SUPPORT_FLIP_COMPRESSED_IMAGE
#	if defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
#		pragma message("Incomplete flip ETC compressed image implement.")
#	else
#		warning "Incomplete flip ETC compressed image implement."
#	endif
#endif

#ifndef max
#define max(a,b) ((a > b) ? a : b)
#endif
//*************************************************************************************************************************
//kiểm tra số nguyên có phải là 1 lũy thừa cũa 2 không,lưu số mũ của lũy thừa của 2 nhỏ nhất kế tiếp số nguyên cần kiểm tra
//nếu số nguyên không là lũy thừa của 2, hoặc là log2 của số nguyên đó nếu nó là lũy thừa của 2, vào biến mà pExponent trỏ đến
//*************************************************************************************************************************
namespace ImgLoaderHelper
{
	bool IsPowerOfTwo(const hq_uint32 d,hq_uint32 *pExponent)
	{
		hq_uint32 i=d;
		if(i==0)
		{
			if(pExponent)
				*pExponent=0;
			return false;
		}
		hq_uint32 result=31;
		if((i & 0xffffff00) == 0)
		{
			i <<= 24;
			result=7;
		}
		else if ((i & 0xffff0000) == 0)
		{
			i <<=16;
			result=15;
		}
		else if ((i & 0xff000000) == 0)
		{
			i <<=8;
			result=23;
		}

		if((i & 0xf0000000)==0)
		{
			i <<=4;
			result-=4;
		}
		while ((i & 0x80000000) == 0)
		{
			i <<=1;
			result-=1;
		}
		if( i & 0x7fffffff )
		{
			if(pExponent)
				*pExponent=result+1;
			return false;
		}
		if(pExponent)
			*pExponent=result;
		return true;
	}
}
/*----------------------------------*/
Bitmap::Bitmap(){
	pData=0;
	pTemp=0;
	width=height=0;
	bits=0;
	imgSize=0;
	origin=ORIGIN_BOTTOM_LEFT;
	ftype=IMG_UNKNOWN;
	format=FMT_UNKNOWN;
	memset(&complex,0,sizeof(SurfaceComplexity));
	layout = LAYOUT_DONT_CARE;
	layout16 = LAYOUT_DONT_CARE;

#if CUBIC_INTERPOLATION
	//clear cubic interpolation cache
	this->InvalidateCubicCache();
#endif
	this->stream = new ImgByteStream();

	this->pDataOwner = true;
}
Bitmap::Bitmap(const Bitmap& source)
{
	this->pData = HQ_NEW hqubyte8[source.GetImgSize()];

	memcpy(pData,source.GetPixelData(),source.GetImgSize());
	this->imgSize = source.imgSize;
	this->pTemp=0;
	this->width=source.GetWidth();
	this->height=source.GetHeight();
	this->bits=source.GetBits();
	this->format=source.GetSurfaceFormat();
	this->origin=source.GetPixelOrigin();
	this->ftype = source.ftype;
	this->complex=source.GetSurfaceComplex();
	layout = LAYOUT_DONT_CARE;
	layout16 = LAYOUT_DONT_CARE;

#if CUBIC_INTERPOLATION
	//clear cubic interpolation cache
	this->InvalidateCubicCache();
#endif
	this->stream = new ImgByteStream();

	this->pDataOwner = true;
}
Bitmap::Bitmap(const hq_ubyte8* pPixelData,hq_uint32 width,
		hq_uint32 height,hq_short16 bits, hquint32 imgSize,
		SurfaceFormat format,ImgOrigin origin,
		SurfaceComplexity &surfaceComplex)
{
	this->pData=(hqubyte8*)pPixelData;
	this->pTemp = NULL;
	this->width=width;
	this->height=height;
	this->bits=bits;
	this->format=format;
	this->origin=origin;
	this->complex=surfaceComplex;
	this->imgSize = imgSize;
	layout = LAYOUT_DONT_CARE;
	layout16 = LAYOUT_DONT_CARE;

#if CUBIC_INTERPOLATION
	//clear cubic interpolation cache
	this->InvalidateCubicCache();
#endif
	this->stream = new ImgByteStream();
	this->pDataOwner = false;
}


void Bitmap::Wrap(const hq_ubyte8* pPixelData,hq_uint32 width,
		hq_uint32 height,hq_short16 bits, hquint32 imgSize,
		SurfaceFormat format,ImgOrigin origin,
		SurfaceComplexity &surfaceComplex)
{
	ClearData();
	pData=(hqubyte8*)pPixelData;
	this->width=width;
	this->height=height;
	this->bits=bits;
	this->format=format;
	this->origin=origin;
	this->complex=surfaceComplex;
	this->imgSize = imgSize;

#if CUBIC_INTERPOLATION
	//clear cubic interpolation cache
	this->InvalidateCubicCache();
#endif

	this->pDataOwner = false;
}

void Bitmap::Set(hq_ubyte8* pPixelData,hq_uint32 width,
		hq_uint32 height,hq_short16 bits, hquint32 imgSize,
		SurfaceFormat format,ImgOrigin origin,
		SurfaceComplexity &surfaceComplex)
{
	ClearData();
	pData=(hqubyte8*)pPixelData;
	this->width=width;
	this->height=height;
	this->bits=bits;
	this->format=format;
	this->origin=origin;
	this->complex=surfaceComplex;
	this->imgSize = imgSize;

#if CUBIC_INTERPOLATION
	//clear cubic interpolation cache
	this->InvalidateCubicCache();
#endif

	this->pDataOwner = true;
}

Bitmap::~Bitmap()
{
	ClearData();
	delete this->stream;
}
//*************************************************
//load file ảnh,lưu dữ liệu pixel của ảnh vào pData
//*************************************************
int Bitmap::LoadFromStream(HQDataReaderStream* dataStream)
{
	//xóa dữ liệu trước đó
	this->ClearData();

	stream->CreateByteStreamFromStream(dataStream);
	return this->LoadPixelData();
}

int Bitmap::LoadFromMemory(const hq_ubyte8 *pImgFileData, hq_uint32 size)
{
	if (pImgFileData == NULL || size == 0)
		return IMG_FAIL_FILE_NOT_EXIST;
	//xóa dữ liệu trước đó
	this->ClearData();
	
	stream->CreateByteStreamFromMemory(pImgFileData, size);

	int result = this->LoadPixelData();

	stream->Clear();

	return result;
}

int Bitmap::LoadPixelData()
{
	ftype=this->CheckfileFormat();
	
	int result;
	switch(ftype)
	{
	case IMG_TGA://tga file
		try{
			pTemp = HQ_NEW hqubyte8[stream->GetStreamSize()];
			stream->GetBytes(pTemp, stream->GetStreamSize());
		}
		catch(std::bad_alloc e)
		{
			return IMG_FAIL_MEM_ALLOC;
		}

		result = this->LoadTGA();
		if(result!=IMG_OK)
		{
			return result;
		}
		break;
	case IMG_BMP://bmp file
		result = this->LoadBMP();
		if(result!=IMG_OK)
		{
			return result;
		}
		break;
	case IMG_DDS://dds file
		result= this->LoadDDS( );
		if(result!=IMG_OK)
		{
			return result;
		}
		break;
	case IMG_PNG:
		result = pngImg.Load(*stream, pData, width, height, bits, imgSize, format, layout);
		if(result!=IMG_OK)
		{
			return result;
		}
		this->origin= ORIGIN_TOP_LEFT;

		break;
	case IMG_JPEG:
		{
			bool greyscale;
			result = jpegImg.Load(*stream, pData, width, height, bits, imgSize, greyscale);
			if (result != IMG_OK)
				return result;
			else
			{
				this->origin= ORIGIN_TOP_LEFT;
				if (greyscale)
				{
					switch (bits)
					{
					case 8 :
						format = FMT_L8;
						break;
					default:
						return IMG_FAIL_NOT_SUPPORTED;
					}
				}
				else
				{
					switch (bits)
					{
					case 24 :
						format=FMT_B8G8R8;
						if (this->layout == LAYOUT_RGB)
							this->FlipRGB();
						break;
					default:
						return IMG_FAIL_NOT_SUPPORTED;
					}
				}
			}
		}
		break;
	case IMG_KTX:
		return this->LoadKTX();
		break;
	case IMG_PVR:
		return this->LoadPVR();
	default:
		return IMG_FAIL_BAD_FORMAT;
	}
	
	if (format == FMT_UNKNOWN)
		return IMG_FAIL_BAD_FORMAT;

	return IMG_OK;
}

void Bitmap::DeletePixelData()
{
	if (pData)
	{
		if (pDataOwner)
			delete[] pData;
		pData = NULL;
	}
	pDataOwner = true;
}

void Bitmap::CopyPixelDataIfNotOwner()
{
	if (pDataOwner)//already is the owner of pixel data pointer
		return;

	if (pData)
	{
		hqubyte8* myOwnData = new hqubyte8[GetImgSize()];

		memcpy(myOwnData, pData, GetImgSize());
		
		pData = myOwnData;
	}

	pDataOwner = true;
}

//********************************
//Load cube map faces
//********************************
int Bitmap::LoadCubeFaces(HQDataReaderStream* dataStreams[6] , ImgOrigin origin , bool generateMipmaps)
{
	this->ClearData();

	Bitmap bitmap[6];//dùng để load 6 file
	SurfaceComplexity complex;
	SurfaceFormat format = FMT_UNKNOWN;//pixel format
	hq_uint32 w , h;//chiều rộng , cao của file ảnh
	hq_uint32 cubeWidth = 0;//chiều rộng của cube map kết quả
	bool needResize;
	bool generateMipFailed = false;
	int re;
	for(int i = 0 ; i < 6 ; ++i)
	{
		re = bitmap[i].LoadFromStream(dataStreams[i]);
		if(re != IMG_OK)
			return re;
		if(format == FMT_UNKNOWN)//format chưa xác định
			format = bitmap[i].GetSurfaceFormat();
		else if (format != bitmap[i].GetSurfaceFormat())//không cùng format
			return IMG_FAIL_CUBE_MAP_LOAD_NOT_SAME_FORMAT;

		complex = bitmap[i].GetSurfaceComplex();
		if (complex.nMipMap > 1 || (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE) !=0 ||
			(complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME) !=0)
			return IMG_FAIL_NOT_SUPPORTED;

		needResize = false;
		w = bitmap[i].GetWidth();
		h = bitmap[i].GetHeight();
		if (w != h)//chiều dài và rộng phải bằng nhau
		{
			needResize = true;
			w = max(w , h);
		}
		if(cubeWidth == 0)
			cubeWidth = w;
		else if (cubeWidth != w)
		{
			needResize = true;
			w = cubeWidth;
		}
		if (needResize)
		{
			if (bitmap[i].DeCompressDXT() == IMG_FAIL_MEM_ALLOC)
				return IMG_FAIL_MEM_ALLOC;
			re = bitmap[i].Scalei(w , w);
			if (re != IMG_OK)
				return re;
		}
		bitmap[i].SetPixelOrigin(origin);
		if (generateMipmaps)
		{
			if (bitmap[i].DeCompressDXT() == IMG_FAIL_MEM_ALLOC)
				return IMG_FAIL_MEM_ALLOC;
			if(bitmap[i].GenerateMipmaps()!=IMG_OK)
				generateMipFailed = true;
		}

		if( i == 0 )
		{
			this->format = bitmap[0].GetSurfaceFormat();
			this->bits = bitmap[0].GetBits();
			this->width = bitmap[0].GetWidth();
			this->height = bitmap[0].GetHeight();
			this->imgSize = 6 * bitmap[0].GetImgSize();
			this->pData = HQ_NEW hq_ubyte8[this->imgSize];
			if(this->pData == NULL)
				return IMG_FAIL_MEM_ALLOC;
		}

		/*---copy face's pixel data-----*/
		memcpy(this->pData + i * bitmap[i].GetImgSize() , 
			bitmap[i].GetPixelData() , bitmap[i].GetImgSize());

		bitmap[i].ClearData();
	}
	
	this->origin = origin;
	this->complex.dwComplexFlags = (SURFACE_COMPLEX_CUBE | SURFACE_COMPLEX_MIPMAP |
		SURFACE_COMPLEX_CUBE_POS_X | SURFACE_COMPLEX_CUBE_POS_Y | SURFACE_COMPLEX_CUBE_POS_Z |
		SURFACE_COMPLEX_CUBE_NEG_X | SURFACE_COMPLEX_CUBE_NEG_Y | SURFACE_COMPLEX_CUBE_NEG_Z
		);
	if( ImgLoaderHelper::IsPowerOfTwo(this->width , &this->complex.nMipMap))
		this->complex.nMipMap ++;

	if(generateMipFailed)
		return IMG_FAIL_CANT_GENERATE_MIPMAPS;
	return IMG_OK;
}

int Bitmap::LoadCubeFacesFromMemory(const hq_ubyte8 *fileDatas[6] , hq_uint32 fileSizes[6], ImgOrigin origin , bool generateMipmaps)
{
	this->ClearData();

	Bitmap bitmap[6];//dùng để load 6 file
	SurfaceComplexity complex;
	SurfaceFormat format = FMT_UNKNOWN;//pixel format
	hq_uint32 w , h;//chiều rộng , cao của file ảnh
	hq_uint32 cubeWidth = 0;//chiều rộng của cube map kết quả
	bool needResize;
	bool generateMipFailed = false;
	int re;
	for(int i = 0 ; i < 6 ; ++i)
	{
		re = bitmap[i].LoadFromMemory(fileDatas[i] , fileSizes[i]);
		if(re != IMG_OK)
			return re;
		if(format == FMT_UNKNOWN)//format chưa xác định
			format = bitmap[i].GetSurfaceFormat();
		else if (format != bitmap[i].GetSurfaceFormat())//không cùng format
			return IMG_FAIL_CUBE_MAP_LOAD_NOT_SAME_FORMAT;

		complex = bitmap[i].GetSurfaceComplex();
		if (complex.nMipMap > 1 || (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE) !=0 ||
			(complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME) !=0)
			return IMG_FAIL_NOT_SUPPORTED;

		needResize = false;
		w = bitmap[i].GetWidth();
		h = bitmap[i].GetHeight();
		if (w != h)//chiều dài và rộng phải bằng nhau
		{
			needResize = true;
			w = max(w , h);
		}
		if(cubeWidth == 0)
			cubeWidth = w;
		else if (cubeWidth != w)
		{
			needResize = true;
			w = cubeWidth;
		}
		if (needResize)
		{
			if (bitmap[i].DeCompressDXT() == IMG_FAIL_MEM_ALLOC)
				return IMG_FAIL_MEM_ALLOC;
			re = bitmap[i].Scalei(w , w);
			if (re != IMG_OK)
				return re;
		}
		bitmap[i].SetPixelOrigin(origin);
		if (generateMipmaps)
		{
			if (bitmap[i].DeCompressDXT() == IMG_FAIL_MEM_ALLOC)
				return IMG_FAIL_MEM_ALLOC;
			if(bitmap[i].GenerateMipmaps()!=IMG_OK)
				generateMipFailed = true;
		}

		if( i == 0 )
		{
			this->format = bitmap[0].GetSurfaceFormat();
			this->bits = bitmap[0].GetBits();
			this->width = bitmap[0].GetWidth();
			this->height = bitmap[0].GetHeight();
			this->imgSize = 6 * bitmap[0].GetImgSize();
			this->pData = HQ_NEW hq_ubyte8[this->imgSize];
			if(this->pData == NULL)
				return IMG_FAIL_MEM_ALLOC;
		}

		/*---copy face's pixel data-----*/
		memcpy(this->pData + i * bitmap[i].GetImgSize() , 
			bitmap[i].GetPixelData() , bitmap[i].GetImgSize());

		bitmap[i].ClearData();
	}
	
	this->origin = origin;
	this->complex.dwComplexFlags = (SURFACE_COMPLEX_CUBE | SURFACE_COMPLEX_MIPMAP |
		SURFACE_COMPLEX_CUBE_POS_X | SURFACE_COMPLEX_CUBE_POS_Y | SURFACE_COMPLEX_CUBE_POS_Z |
		SURFACE_COMPLEX_CUBE_NEG_X | SURFACE_COMPLEX_CUBE_NEG_Y | SURFACE_COMPLEX_CUBE_NEG_Z
		);
	if( ImgLoaderHelper::IsPowerOfTwo(this->width , &this->complex.nMipMap))
		this->complex.nMipMap ++;

	if(generateMipFailed)
		return IMG_FAIL_CANT_GENERATE_MIPMAPS;
	return IMG_OK;
}
#if CUBIC_INTERPOLATION
void Bitmap::InvalidateCubicCache()
{
	for (int i = 0 ; i < 4 ;++i)
		this->cubicCell[i] = -1.f;
}
#endif

//********************************
//xóa dữ liệu pixel và dữ liệu tạm
//********************************
void Bitmap::ClearData()
{
	if(pTemp)
	{
		delete[] pTemp;
		pTemp=0;
	}
	DeletePixelData();
	width=height=0;
	bits=0;
	imgSize=0;
	origin=ORIGIN_BOTTOM_LEFT;
	ftype=IMG_UNKNOWN;
	format=FMT_UNKNOWN;
	memset(&complex,0,sizeof(SurfaceComplexity));

	jpegImg.Clear();
	pngImg.Clear();
	stream->Clear();
#if CUBIC_INTERPOLATION
	//clear cubic interpolation cache
	this->InvalidateCubicCache();
#endif
}
//***********************************
//Flip theo chiều dọc
//***********************************
int Bitmap::FlipVertical()
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format==FMT_UNKNOWN || format == FMT_PVRTC_RGB_2BPP ||
		format == FMT_PVRTC_RGB_4BPP || format == FMT_PVRTC_RGBA_2BPP ||
		format == FMT_PVRTC_RGBA_4BPP
#if !SUPPORT_FLIP_COMPRESSED_IMAGE
		|| format == FMT_ETC1 || format == FMT_S3TC_DXT1 ||
		format == FMT_S3TC_DXT3 || format == FMT_S3TC_DXT5
#endif
		)
		return IMG_FAIL_NOT_SUPPORTED;
	if( complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
		return IMG_FAIL_NOT_SUPPORTED;

	this->CopyPixelDataIfNotOwner();
	
	hq_uint32 w;
	hq_uint32 h;
	
	hq_ubyte8 *pRow1,*pRow2;
	hq_uint32 rowSize;

	hquint32 faces = 1;
	if (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE)
		faces = 6;
	hq_uint32 nMip=(complex.nMipMap>0)? complex.nMipMap : 1;//số mipmap level
	hq_ubyte8 *pCur=pData;
	
	for (hquint32 face = 0 ; face < faces ; ++face)
	{
		w=width;
		h=height;
		for(hq_uint32 mipLevel=0;mipLevel<nMip;++mipLevel)
		{
			if(h<2)//không cần Flip khi chiều cao chỉ là 1
			{
				pCur += this->CalculateSize(w, h);//next level/face
			}
			else
			{
				switch(format)
				{
#if SUPPORT_FLIP_COMPRESSED_IMAGE
				case FMT_ETC1:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*8;
						pRow1=pCur;//hàng block đầu tiên
						if(nHeightBlock==1)//chỉ có 1 hàng block
						{
							for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
							{
								if(h>2)
									FlipFullBlockETC1(pRow1);
								else
									FlipHalfBlockETC1(pRow1);
								pRow1+=8;//next block
							}
						}
						else{//nhiều hơn 1 hàng block
							pRow2=pCur+(nHeightBlock-1)*rowSize;//hàng block cuối
							for(;pRow1<pRow2;pRow2-=2*rowSize)
							{
								for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
								{
									SwapBlockETC1(pRow1,pRow2);
									FlipFullBlockETC1(pRow1);
									FlipFullBlockETC1(pRow2);
									pRow1+=8;//next block
									pRow2+=8;//next block
								}
							}
						}

						pCur+=nWidthBlock*nHeightBlock*8;//next mipmap level
					}
					break;
				case FMT_S3TC_DXT1:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*8;
						pRow1=pCur;//hàng block đầu tiên
						if(nHeightBlock==1)//chỉ có 1 hàng block
						{
							for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
							{
								if(h>2)
									FlipFullBlockDXT1(pRow1);
								else
									FlipHalfBlockDXT1(pRow1);
								pRow1+=8;//next block
							}
						}
						else{//nhiều hơn 1 hàng block
							pRow2=pCur+(nHeightBlock-1)*rowSize;//hàng block cuối
							for(;pRow1<pRow2;pRow2-=2*rowSize)
							{
								for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
								{
									SwapBlockDXT1(pRow1,pRow2);
									FlipFullBlockDXT1(pRow1);
									FlipFullBlockDXT1(pRow2);
									pRow1+=8;//next block
									pRow2+=8;//next block
								}
							}
						}

						pCur+=nWidthBlock*nHeightBlock*8;//next mipmap level
					}
					break;
				case FMT_S3TC_DXT3:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*16;
						pRow1=pCur;//hàng block đầu tiên
						if(nHeightBlock==1)//chỉ có 1 hàng block
						{
							for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
							{
								if(h>2)
									FlipFullBlockDXT3(pRow1);
								else
									FlipHalfBlockDXT3(pRow1);
								pRow1+=16;//next block
							}
						}
						else{//nhiều hơn 1 hàng block
							pRow2=pCur+(nHeightBlock-1)*rowSize;//hàng block cuối
							for(;pRow1<pRow2;pRow2-=2*rowSize)
							{
								for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
								{
									SwapBlockDXT3_5(pRow1,pRow2);
									FlipFullBlockDXT3(pRow1);
									FlipFullBlockDXT3(pRow2);
									pRow1+=16;//next block
									pRow2+=16;//next block
								}
							}
						}

						pCur+=nWidthBlock*nHeightBlock*16;//next mipmap level
					}
					break;
				case FMT_S3TC_DXT5:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*16;
						pRow1=pCur;//hàng block đầu tiên
						if(nHeightBlock==1)//chỉ có 1 hàng block
						{
							for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
							{
								if(h>2)
									FlipFullBlockDXT5(pRow1);
								else
									FlipHalfBlockDXT5(pRow1);
								pRow1+=16;//next block
							}
						}
						else{//nhiều hơn 1 hàng block
							pRow2=pCur+(nHeightBlock-1)*rowSize;//hàng block cuối
							for(;pRow1<pRow2;pRow2-=2*rowSize)
							{
								for(hq_uint32 wBlock=0;wBlock<nWidthBlock;++wBlock)
								{
									SwapBlockDXT3_5(pRow1,pRow2);
									FlipFullBlockDXT5(pRow1);
									FlipFullBlockDXT5(pRow2);
									pRow1+=16;//next block
									pRow2+=16;//next block
								}
							}
						}

						pCur+=nWidthBlock*nHeightBlock*16;//next mipmap level
					}
					break;
#endif
				default:
					{
						hq_ubyte8 temp;
						rowSize=w*bits/8;//độ lớn (bytes) của 1 hàng

						pRow1=pCur;//hàng đầu tiên
						pRow2=pCur+rowSize*(h-1);//hàng cuối cùng

						//đảo lần lượt từng hàng trên dưới
						for(;pRow1<pRow2;pRow2-=2*rowSize)
						{
							for(hq_uint32 index=0;index<rowSize;++pRow1,++pRow2,++index)//đảo lần lượt từng pixel ở 2 hàng
							{
								temp=*pRow1;
								*pRow1=*pRow2;
								*pRow2=temp;
							}
						}

						pCur+=rowSize*h;//next level
					}
				}//switch
			}//if (h >= 2)
			w >>=1;
			h >>=1;
			if(w==0)
				w=1;
			if(h==0)
				h=1;
		}//for mip level
	}//for face
	switch(origin)
	{
	case ORIGIN_TOP_LEFT:
		origin=ORIGIN_BOTTOM_LEFT;
		break;
	case ORIGIN_TOP_RIGHT:
		origin=ORIGIN_BOTTOM_RIGHT;
		break;
	case ORIGIN_BOTTOM_LEFT:
		origin=ORIGIN_TOP_LEFT;
		break;
	case ORIGIN_BOTTOM_RIGHT:
		origin=ORIGIN_TOP_RIGHT;
		break;
	}

	return IMG_OK;
}
//*****************************
//Flip theo chiều ngang
//*****************************
int Bitmap::FlipHorizontal()
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format==FMT_UNKNOWN || format == FMT_PVRTC_RGB_2BPP ||
		format == FMT_PVRTC_RGB_4BPP || format == FMT_PVRTC_RGBA_2BPP ||
		format == FMT_PVRTC_RGBA_4BPP
#if !SUPPORT_FLIP_COMPRESSED_IMAGE
		|| format == FMT_ETC1 || format == FMT_S3TC_DXT1 ||
		format == FMT_S3TC_DXT3 || format == FMT_S3TC_DXT5
#endif		
		)
		return IMG_FAIL_NOT_SUPPORTED;
	if( complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
		return IMG_FAIL_NOT_SUPPORTED;
	
	this->CopyPixelDataIfNotOwner();

	hq_ubyte8 *pIterator1,*pIterator2,*pRow;
	hq_ubyte8 *temp;

	hq_ushort16 pixelSize=bits/8;
	temp=new hq_ubyte8[pixelSize];

	hq_uint32 rowSize;//độ lớn (bytes) của 1 hàng
	hq_ubyte8 *pEnd;//địa chỉ kết thúc 1 mipmap level
	hq_uint32 w;
	hq_uint32 h;

	pRow=pData;//địa chỉ bắt đầu

	hquint32 faces = 1;
	if (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE)
		faces = 6;
	
	hq_uint32 nMip=(complex.nMipMap>0)? complex.nMipMap : 1;//số mipmap level
	
	for (hquint32 face = 0 ; face < faces ; ++face)
	{
		w=width;
		h=height;
		for(hq_uint32 mipLevel=0;mipLevel<nMip;++mipLevel)
		{
			if(w<2)//chiều rộng chỉ là 1 không cần Flip
			{
				pRow += this->CalculateSize(w, h);//next level/face
			}
			else
			{
				switch(format)
				{
#if SUPPORT_FLIP_COMPRESSED_IMAGE
				case FMT_ETC1:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*8;
						if (nWidthBlock==1)//chỉ có 1 cột block
						{
							for(hq_uint32 hBlock=0;hBlock<nHeightBlock;++hBlock)
							{
								if(w>2)
									FlipHFullBlockETC1(pRow);
								else
									FlipHHalfBlockETC1(pRow);
								pRow+=8;//next block
							}
						}
						else
						{
							pEnd=pRow + rowSize*nHeightBlock;
							while(pRow<pEnd){
								pIterator1=pRow;
								pIterator2=pRow+rowSize-8;
								for (;pIterator1<pIterator2;pIterator1+=8,pIterator2-=8)
								{
									SwapBlockETC1(pIterator1,pIterator2);
									FlipHFullBlockETC1(pIterator1);
									FlipHFullBlockETC1(pIterator2);
								}

								pRow+=rowSize;//next height
							}
						}
					}
					break;
				case FMT_S3TC_DXT1:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*8;
						if (nWidthBlock==1)//chỉ có 1 cột block
						{
							for(hq_uint32 hBlock=0;hBlock<nHeightBlock;++hBlock)
							{
								if(w>2)
									FlipHFullBlockDXT1(pRow);
								else
									FlipHHalfBlockDXT1(pRow);
								pRow+=8;//next block
							}
						}
						else
						{
							pEnd=pRow + rowSize*nHeightBlock;
							while(pRow<pEnd){
								pIterator1=pRow;
								pIterator2=pRow+rowSize-8;
								for (;pIterator1<pIterator2;pIterator1+=8,pIterator2-=8)
								{
									SwapBlockDXT1(pIterator1,pIterator2);
									FlipHFullBlockDXT1(pIterator1);
									FlipHFullBlockDXT1(pIterator2);
								}

								pRow+=rowSize;//next height
							}
						}
					}
					break;
				case FMT_S3TC_DXT3:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*16;
						if (nWidthBlock==1)//chỉ có 1 cột block
						{
							for(hq_uint32 hBlock=0;hBlock<nHeightBlock;++hBlock)
							{
								if(w>2)
									FlipHFullBlockDXT3(pRow);
								else
									FlipHHalfBlockDXT3(pRow);
								pRow+=16;//next block
							}
						}
						else
						{
							pEnd=pRow + rowSize*nHeightBlock;
							while(pRow<pEnd){
								pIterator1=pRow;
								pIterator2=pRow+rowSize-16;
								for (;pIterator1<pIterator2;pIterator1+=16,pIterator2-=16)
								{
									SwapBlockDXT3_5(pIterator1,pIterator2);
									FlipHFullBlockDXT3(pIterator1);
									FlipHFullBlockDXT3(pIterator2);
								}

								pRow+=rowSize;//next height
							}
						}
					}
					break;
				case FMT_S3TC_DXT5:
					{
						hq_uint32 nWidthBlock=(w+3)/4;//số block 4x4 trên 1 hàng
						hq_uint32 nHeightBlock=(h+3)/4;//số block 4x4 trên 1 cột
						rowSize=nWidthBlock*16;
						if (nWidthBlock==1)//chỉ có 1 cột block
						{
							for(hq_uint32 hBlock=0;hBlock<nHeightBlock;++hBlock)
							{
								if(w>2)
									FlipHFullBlockDXT5(pRow);
								else
									FlipHHalfBlockDXT5(pRow);
								pRow+=16;//next block
							}
						}
						else
						{
							pEnd=pRow + rowSize*nHeightBlock;
							while(pRow<pEnd){
								pIterator1=pRow;
								pIterator2=pRow+rowSize-16;
								for (;pIterator1<pIterator2;pIterator1+=16,pIterator2-=16)
								{
									SwapBlockDXT3_5(pIterator1,pIterator2);
									FlipHFullBlockDXT5(pIterator1);
									FlipHFullBlockDXT5(pIterator2);
								}

								pRow+=rowSize;//next height
							}
						}
					}
					break;
#endif
				default:
					rowSize=w*pixelSize;
					pEnd=pRow + rowSize*h;
					while(pRow<pEnd)
					{
						//đảo lần lượt pixel trên 1 hàng
						pIterator1=pRow;
						pIterator2=pRow+rowSize-pixelSize;
						for (;pIterator1<pIterator2;pIterator1+=pixelSize,pIterator2-=pixelSize){
							memcpy(temp,pIterator1,pixelSize);
							memcpy(pIterator1,pIterator2,pixelSize);
							memcpy(pIterator2,temp,pixelSize);
						}
						pRow+=rowSize;
					}
				}//switch
			}//if (w >= 2)
			w >>=1;
			h >>=1;
			if(h==0)
				h=1;
			if(w==0)
				w=1;
		}//for miplevel
	}//for face
	delete[] temp;
	
	switch(origin)
	{
	case ORIGIN_TOP_LEFT:
		origin=ORIGIN_TOP_RIGHT;
		break;
	case ORIGIN_TOP_RIGHT:
		origin=ORIGIN_TOP_LEFT;
		break;
	case ORIGIN_BOTTOM_LEFT:
		origin=ORIGIN_BOTTOM_RIGHT;
		break;
	case ORIGIN_BOTTOM_RIGHT:
		origin=ORIGIN_BOTTOM_LEFT;
		break;
	}

	return IMG_OK;
}
//***************************
//chỉnh lại vi trí pixel đầu tiên
//***************************
int Bitmap::SetPixelOrigin(ImgOrigin targetOrigin)
{
	int result;
	switch(targetOrigin)
	{
	case ORIGIN_TOP_LEFT:
		switch(origin)
		{
		case ORIGIN_TOP_RIGHT:
			return this->FlipHorizontal();
			break;
		case ORIGIN_BOTTOM_LEFT:
			return this->FlipVertical();
			break;
		case ORIGIN_BOTTOM_RIGHT:
			result = this->FlipHorizontal();
			if (result == IMG_OK)
				return this->FlipVertical();
			else return result;
			break;
		}
		break;
	case ORIGIN_TOP_RIGHT:
		switch(origin)
		{
		case ORIGIN_TOP_LEFT:
			return this->FlipHorizontal();
			break;
		case ORIGIN_BOTTOM_RIGHT:
			return this->FlipVertical();
			break;
		case ORIGIN_BOTTOM_LEFT:
			result = this->FlipHorizontal();
			if (result == IMG_OK)
				return this->FlipVertical();
			else return result;
			break;
		}
		break;
	case ORIGIN_BOTTOM_LEFT:
		switch(origin)
		{
		case ORIGIN_BOTTOM_RIGHT:
			return this->FlipHorizontal();
			break;
		case ORIGIN_TOP_LEFT:
			return this->FlipVertical();
			break;
		case ORIGIN_TOP_RIGHT:
			result = this->FlipHorizontal();
			if (result == IMG_OK)
				return this->FlipVertical();
			else return result;
			break;
		}
		break;
	case ORIGIN_BOTTOM_RIGHT:
		switch(origin)
		{
		case ORIGIN_BOTTOM_LEFT:
			return this->FlipHorizontal();
			break;
		case ORIGIN_TOP_RIGHT:
			return this->FlipVertical();
			break;
		case ORIGIN_TOP_LEFT:
			result = this->FlipHorizontal();
			if (result == IMG_OK)
				return this->FlipVertical();
			else return result;
			break;
		}
		break;
	}
	return IMG_OK;
}
//*****************
//đọc đuôi file
//*****************
FileType Bitmap::CheckfileFormat()
{
	//read magic numbers first
	if(stream->GetByte() =='B' && stream->GetByte()=='M')
	{
		stream->Rewind();
		return IMG_BMP;
	}
	stream->Rewind();
	if(stream->GetByte()=='D' && stream->GetByte()=='D'&&
		stream->GetByte()=='S' && stream->GetByte()==' ')
	{
		stream->Rewind();
		return IMG_DDS;
	}
	stream->Rewind();

	if (this->IsKTX())
	{
		return IMG_KTX;
	}
	stream->Rewind();


	//check if this is tga file
	if (stream->GetStreamSize() > 26)
	{
		hq_ubyte8 footer[26];
		stream->Seek(stream->GetStreamSize() - 26);
		stream->GetBytes(footer, 26);

		if(!memcmp(&footer[8],"TRUEVISION-XFILE",16))
		{
			stream->Rewind();
			return IMG_TGA;
		}
	}
	stream->Rewind();
	//check if this is pvr
	if (stream->GetStreamSize() >= 13 * sizeof(hquint32))
	{
		hquint32 headerSize;
		stream->GetBytes(&headerSize, 4);
		if (headerSize == 52)
		{
			hqbyte8 tag[4];
			stream->Seek(11 * 4);
			stream->GetBytes(tag, 4);
			if (!strncmp(tag, "PVR!", 4))
			{
				stream->Rewind();
				return IMG_PVR;
			}
		}
	}
	stream->Rewind();
	//check if this is png
	if (pngImg.IsPNG(*stream))
	{
		return IMG_PNG;
	}
	
	stream->Rewind();

	//check if this is jpeg
	if (jpegImg.IsJPEG(*stream))
		return IMG_JPEG;//no rewind stream

	return IMG_UNKNOWN;
}
int Bitmap::FlipRGB()
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;

	this->CopyPixelDataIfNotOwner();

	hq_uint32 pixelSize=bits/8;
	hq_ubyte8 *cur=pData;
	hq_ubyte8* pEnd=pData+imgSize;
	hq_ubyte8 temp;
	if(format==FMT_R8G8B8||format==FMT_B8G8R8 ||
		format==FMT_A8R8G8B8||format==FMT_X8R8G8B8 || 
		format==FMT_A8B8G8R8 || format==FMT_X8B8G8R8){
		for(;cur<pEnd;cur+=pixelSize)//với từng pixel
		{
			//đảo giá trị red và blue
			temp=cur[0];
			cur[0]=cur[2];
			cur[2]=temp;
		}
		
		switch(format)
		{
		case FMT_R8G8B8:
			format = FMT_B8G8R8;
			break;
		case FMT_B8G8R8:
			format = FMT_R8G8B8;
			break;
		case FMT_A8R8G8B8:
			format = FMT_A8B8G8R8;
			break;
		case FMT_X8R8G8B8:
			format = FMT_X8B8G8R8;
			break;
		case FMT_A8B8G8R8:
			format = FMT_A8R8G8B8;
			break;
		case FMT_X8B8G8R8:
			format = FMT_X8R8G8B8;
			break;
		}

		return IMG_OK;
	}
	if(	format==FMT_R8G8B8A8 || 
		format==FMT_B8G8R8A8 )
	{
		for(;cur<pEnd;cur+=pixelSize)//với từng pixel
		{
			//đảo giá trị red và blue
			temp=cur[1];
			cur[1]=cur[3];
			cur[3]=temp;
		}

		if (format == FMT_R8G8B8A8)
			format = FMT_B8G8R8A8;
		else
			format = FMT_R8G8B8A8;

		return IMG_OK;
	}
	if(format==FMT_R5G6B5 || format == FMT_B5G6R5){ 
		for(;cur<pEnd;cur+=pixelSize)//với từng pixel
		{
			//đảo giá trị red và blue
			hqushort16 oldColor = *((hqushort16*)cur);
			*((hqushort16*)cur) = SwapRGB16(oldColor);
		}

		if (format == FMT_B5G6R5)
			format = FMT_R5G6B5;
		else
			format = FMT_B5G6R5;
		return IMG_OK;
	}
	return IMG_FAIL_NOT_SUPPORTED;
}

int Bitmap::FlipRGBA()
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_A8R8G8B8&&format!=FMT_A8B8G8R8&&format!=FMT_R8G8B8A8&&format!=FMT_B8G8R8A8)
		return IMG_FAIL_NOT_SUPPORTED;

	this->CopyPixelDataIfNotOwner();

	hq_ubyte8 *cur=pData;
	hq_ubyte8* pEnd=pData+imgSize;
	hq_uint32 temp;//32 bit
	
	for(;cur<pEnd;cur+=4)
	{
		temp=(cur[3])|(cur[2]<<8)|(cur[1]<<16)|(cur[0]<<24);
		*((hq_uint32*)cur)=temp;
	}
	switch(format)
	{
	case FMT_A8R8G8B8:
		format=FMT_B8G8R8A8;
		break;
	case FMT_A8B8G8R8:
		format=FMT_R8G8B8A8;
		break;
	case FMT_R8G8B8A8:
		format=FMT_A8B8G8R8;
		break;
	case FMT_B8G8R8A8:
		format=FMT_A8R8G8B8;
		break;
	}
	return IMG_OK;
}
//******************************
//phóng to thu nhỏ hình ảnh
//******************************

int Bitmap::Scalef(hq_float32 wFactor, hq_float32 hFactor)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;

	if(wFactor==1.0f && hFactor==1.0f)//no change
		return IMG_OK;

	if(complex.dwComplexFlags & SURFACE_COMPLEX_CUBE ||
		complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME ||
		complex.dwComplexFlags & SURFACE_COMPLEX_MIPMAP)
		return IMG_FAIL_NOT_SUPPORTED;//not supported yet


	hq_uint32 newW=(hq_uint32)(width*wFactor);
	hq_uint32 newH=(hq_uint32)(height*hFactor);

	hq_ushort16 pixelSize=bits/8;
	hq_uint32 newSize=newW*newH*pixelSize;
	//bilinear filtering
	
	hq_ubyte8 * newData=new hq_ubyte8[newSize];
	hq_ubyte8 * pPixel=newData;
	if(!newData)
		return IMG_FAIL_MEM_ALLOC;

	hq_float32 wRatio=1.0f/wFactor;// old width / new width
	hq_float32 hRatio=1.0f/hFactor;// old height / new height
	
	hq_uint32 lineSize=pixelSize*width;//độ lớn (bytes) của 1 hàng pixel ảnh
	
	hqfloat32 u,v;

	for(hq_uint32 row=0;row<newH;++row)
	{
		v = (row + 0.5f) * hRatio - 0.5f;
		for(hq_uint32 col=0;col<newW;++col)
		{
			u = (col + 0.5f) * wRatio - 0.5f;
			switch(format)
			{
			case FMT_R8G8B8:case FMT_B8G8R8:
				this->BilinearFiler24(pPixel,u , v ,lineSize);
				break;
			case FMT_A8R8G8B8:case FMT_A8B8G8R8:case FMT_X8R8G8B8:case FMT_X8B8G8R8:case FMT_R8G8B8A8:case FMT_B8G8R8A8:
				this->BilinearFiler32(pPixel,u , v ,lineSize);
				break;
			case FMT_L8:case FMT_A8:
				this->BilinearFiler8(pPixel,u , v ,lineSize);
				break;
			case FMT_A8L8:
				this->BilinearFiler16AL(pPixel,u , v ,lineSize);
				break;
			case FMT_R5G6B5: case FMT_B5G6R5: 
				this->BilinearFiler16RGB(pPixel,u , v ,lineSize);
				break;
			default:
				delete[] newData;
				return IMG_FAIL_NOT_SUPPORTED;
			}
			pPixel+=pixelSize;
		}
	}

	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	
	width=newW;
	height=newH;

	imgSize=newSize;

	return IMG_OK;
}


int Bitmap::Scalei(hq_uint32 newW, hq_uint32 newH)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(newW==width && newH==height)//no change
		return IMG_OK;

	if(complex.dwComplexFlags & SURFACE_COMPLEX_CUBE ||
		complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME ||
		complex.dwComplexFlags & SURFACE_COMPLEX_MIPMAP)
		return IMG_FAIL_NOT_SUPPORTED;//not supported yet
	
	hq_ushort16 pixelSize=bits/8;
	hq_uint32 newSize=newW*newH*pixelSize;
	//bilinear filtering
	
	hq_ubyte8 * newData=new hq_ubyte8[newSize];
	hq_ubyte8 * pPixel=newData;
	if(!newData)
		return IMG_FAIL_MEM_ALLOC;

	hq_float32 wRatio=(hq_float32)width/newW;// old width / new width
	hq_float32 hRatio=(hq_float32)height/newH;// old height / new height
	
	hq_uint32 lineSize=pixelSize*width;//độ lớn (bytes) của 1 hàng pixel ảnh

	hqfloat32 u,v;

	for(hq_uint32 row=0;row<newH;++row)
	{
		v = (row + 0.5f) * hRatio - 0.5f;
		for(hq_uint32 col=0;col<newW;++col)
		{
			u = (col + 0.5f) * wRatio - 0.5f;
			switch(format)
			{
			case FMT_R8G8B8: case FMT_B8G8R8:
				this->BilinearFiler24(pPixel,u , v ,lineSize);
				break;
			case FMT_A8R8G8B8:case FMT_A8B8G8R8:case FMT_X8R8G8B8:case FMT_X8B8G8R8:case FMT_R8G8B8A8:case FMT_B8G8R8A8:
				this->BilinearFiler32(pPixel,u , v ,lineSize);
				break;
			case FMT_L8:case FMT_A8:
				this->BilinearFiler8(pPixel,u , v ,lineSize);
				break;
			case FMT_A8L8:
				this->BilinearFiler16AL(pPixel,u , v ,lineSize);
				break;
			case FMT_R5G6B5: case FMT_B5G6R5:
				this->BilinearFiler16RGB(pPixel,u , v ,lineSize);
				break;
			default:
				delete[] newData;
				return IMG_FAIL_NOT_SUPPORTED;
			}
			pPixel+=pixelSize;
		}
	}

	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;

	width=newW;
	height=newH;

	imgSize=newSize;

	return IMG_OK;
}

#if CUBIC_INTERPOLATION
void Bitmap::CalCubicCofactors(hqfloat32 u, hqfloat32 v, hqfloat32 lineSize, hquint32 channels)
{
	//get cell this point is on
	hqfloat32 x1,x2,y1, y2;

	x1=floorf(u);
	y1=floorf(v);

	x2=x1+1.f;
	y2=y1+1.f;

	if (x2 >= (hqfloat32)width)
		x2 = x1;
	if (y2 >= (hqfloat32) height)
		y2 = y1;

	if (x1 == this->cubicCell[0] && y1 == this->cubicCell[1] &&
		x2 == this->cubicCell[2] && y2 == this->cubicCell[3])//already calculated
		return;

	this->cubicCell[0] = x1;
	this->cubicCell[1] = y1;
	this->cubicCell[2] = x2;
	this->cubicCell[3] = y2;

	hqfloat32 x0,x3,y0,y3;
	x0 = x1 - 1.f;
	x3 = x2 + 1.f;
	y0 = y1 - 1.f;
	y3 = y2 + 1.f;

	if (x0 < 0.f)
		x0 = 0.f;
	if (x3 > (hqfloat32)width)
		x3 = x2;
	if (y0 < 0.f)
		y0 = 0.f;
	if (y3 > (hqfloat32)height)
		y3 = y2;

	
	//cubic interpolation cofactors
	if (channels > 0)
	{
		hqubyte8 * p[4][4];//4x4 pixels
		p[0][0] = this->pData + lineSize * y0 + x0 * channels;
		p[0][1] = this->pData + lineSize * y1 + x0 * channels;
		p[0][2] = this->pData + lineSize * y2 + x0 * channels;
		p[0][3] = this->pData + lineSize * y3 + x0 * channels;

		p[1][0] = this->pData + lineSize * y0 + x1 * channels;
		p[1][1] = this->pData + lineSize * y1 + x1 * channels;
		p[1][2] = this->pData + lineSize * y2 + x1 * channels;
		p[1][3] = this->pData + lineSize * y3 + x1 * channels;

		p[2][0] = this->pData + lineSize * y0 + x2 * channels;
		p[2][1] = this->pData + lineSize * y1 + x2 * channels;
		p[2][2] = this->pData + lineSize * y2 + x2 * channels;
		p[2][3] = this->pData + lineSize * y3 + x2 * channels;

		p[3][0] = this->pData + lineSize * y0 + x3 * channels;
		p[3][1] = this->pData + lineSize * y1 + x3 * channels;
		p[3][2] = this->pData + lineSize * y2 + x3 * channels;
		p[3][3] = this->pData + lineSize * y3 + x3 * channels;

		for (hquint32 i = 0 ; i < channels ; ++i)
		{
			for (hquint32 r = 0 ; r < 4 ; ++r)
				for (hquint32 c = 0 ; c < 4 ; ++c)
					p[r][c] ++;//next channel

			a[i][0] = *p[1][1];
			a[i][1] = -.5f**p[1][0] + .5f**p[1][2];
			a[i][2] = *p[1][0] - 2.5f**p[1][1] + 2.f**p[1][2] - .5f**p[1][3];
			a[i][3] = -.5f**p[1][0] + 1.5f**p[1][1] - 1.5f**p[1][2] + .5f**p[1][3];
			a[i][4] = -.5f**p[0][1] + .5f**p[2][1];
			a[i][5] = .25f**p[0][0] - .25f**p[0][2] - .25f**p[2][0] + .25f**p[2][2];
			a[i][6] = -.5f**p[0][0] + 1.25f**p[0][1] - *p[0][2] + .25f**p[0][3] + .5f**p[2][0] - 1.25f**p[2][1] + *p[2][2] - .25f**p[2][3];
			a[i][7] = .25**p[0][0] - .75f**p[0][1] + .75f**p[0][2] - .25f**p[0][3] - .25f**p[2][0] + .75f**p[2][1] - .75f**p[2][2] + .25f**p[2][3];
			a[i][8] = *p[0][1] - 2.5f**p[1][1] + 2.f**p[2][1] - .5f**p[3][1];
			a[i][9] = -.5f**p[0][0] + .5f**p[0][2] + 1.25f**p[1][0] - 1.25f**p[1][2] - *p[2][0] + *p[2][2] + .25f**p[3][0] - .25f**p[3][2];
			a[i][10] = *p[0][0] - 2.5f**p[0][1] + 2.f**p[0][2] - .5f**p[0][3] - 2.5f**p[1][0] + 6.25f**p[1][1] - 5.f**p[1][2] + 1.25f**p[1][3] + 2.f**p[2][0] - 5.f**p[2][1] + 4.f**p[2][2] - *p[2][3] - .5f**p[3][0] + 1.25f**p[3][1] - *p[3][2] + .25f**p[3][3];
			a[i][11] = -.5f**p[0][0] + 1.5f**p[0][1] - 1.5f**p[0][2] + .5f**p[0][3] + 1.25f**p[1][0] - 3.75f**p[1][1] + 3.75f**p[1][2] - 1.25f**p[1][3] - *p[2][0] + 3.f**p[2][1] - 3.f**p[2][2] + *p[2][3] + .25f**p[3][0] - .75f**p[3][1] + .75f**p[3][2] - .25f**p[3][3];
			a[i][12] = -.5f**p[0][1] + 1.5f**p[1][1] - 1.5f**p[2][1] + .5f**p[3][1];
			a[i][13] = .25f**p[0][0] - .25f**p[0][2] - .75f**p[1][0] + .75f**p[1][2] + .75f**p[2][0] - .75f**p[2][2] - .25f**p[3][0] + .25f**p[3][2];
			a[i][14] = -.5f**p[0][0] + 1.25f**p[0][1] - *p[0][2] + .25f**p[0][3] + 1.5f**p[1][0] - 3.75f**p[1][1] + 3.f**p[1][2] - .75f**p[1][3] - 1.5f**p[2][0] + 3.75f**p[2][1] - 3.f**p[2][2] + .75f**p[2][3] + .5f**p[3][0] - 1.25f**p[3][1] + *p[3][2] - .25f**p[3][3];
			a[i][15] = .25f**p[0][0] - .75f**p[0][1] + .75f**p[0][2] - .25f**p[0][3] - .75f**p[1][0] + 2.25f**p[1][1] - 2.25f**p[1][2] + .75f**p[1][3] + .75f**p[2][0] - 2.25f**p[2][1] + 2.25f**p[2][2] - .75f**p[2][3] - .25f**p[3][0] + .75f**p[3][1] - .75f**p[3][2] + .25f**p[3][3];
		}
#error need implement
}

void Bitmap::CubicFilter(hqubyte8 *pixel, hqfloat32 u, hqfloat32 v, hquint32 lineSize, hquint32 channels)
{
#error need implement
}
#endif

//helper funciton: nội suy tuyến tính
static hq_float32 Lerp(hq_float32 a,hq_float32 b,hq_float32 t)
{
	return (1-t)*a+t*b;
}

void Bitmap::BilinearFiler24(hq_ubyte8 *pPixel, hq_float32 u, hq_float32 v,const hq_uint32 lineSize)
{
	//----------------------
	//c0(x0,y0)------A------------ c1(x1,y0)
	//		|		 |				|
	//		|--------p(u,v)			|
	//		|		 |				|
	//		|		 |				|
	//c2(x0,y1)------B------------ c3(x1,y1)
	//----------------------
	hq_uint32 x0,y0,x1,y1;
	hq_float32 t1,t2;
	hq_ubyte8 *c0,*c1,*c2,*c3;


	x0=(hq_uint32)floorf(u);
	y0=(hq_uint32)floorf(v);

	x1=x0+1;
	y1=y0+1;

	t1=(u-x0);//hệ số nội suy ngang
	t2=(v-y0);//hệ số nội suy dọc

	bool outOfWidth=(x1>=width);
	bool outOfHeight=(y1>=height);
	
	//giá trị pixel ở (x0,y0)
	c0=pData+lineSize*y0+x0*3;
	//giá trị pixel ở (x1,y0)
	if(!outOfWidth)
		c1=pData+lineSize*y0+x1*3;
	else c1=c0;
	//giá trị pixel ở (x0,y1)
	if(!outOfHeight)
		c2=pData+lineSize*y1+x0*3;
	else c2=c0;
	//giá trị pixel ở (x1,y1)
	if(outOfHeight)
		c3=c1;
	else if(outOfWidth)
		c3=c2;
	else
		c3=pData+lineSize*y1+x1*3;
	
	hq_float32 A,B;
	//kênh màu thứ 1
	A=Lerp(c0[0]/255.0f,c1[0]/255.0f,t1);
	B=Lerp(c2[0]/255.0f,c3[0]/255.0f,t1);
	
	pPixel[0]=(hq_ubyte8)(255*Lerp(A,B,t2));

	//kênh màu thứ 2
	A=Lerp(c0[1]/255.0f,c1[1]/255.0f,t1);
	B=Lerp(c2[1]/255.0f,c3[1]/255.0f,t1);
	
	pPixel[1]=(hq_ubyte8)(255*Lerp(A,B,t2));

	//kênh màu thứ 3
	A=Lerp(c0[2]/255.0f,c1[2]/255.0f,t1);
	B=Lerp(c2[2]/255.0f,c3[2]/255.0f,t1);
	
	pPixel[2]=(hq_ubyte8)(255*Lerp(A,B,t2));

}

void Bitmap::BilinearFiler32(hq_ubyte8 *pPixel, hq_float32 u, hq_float32 v,const hq_uint32 lineSize)
{
	//----------------------
	//c0(x0,y0)------A------------ c1(x1,y0)
	//		|		 |				|
	//		|--------p(u,v)			|
	//		|		 |				|
	//		|		 |				|
	//c2(x0,y1)------B------------ c3(x1,y1)
	//----------------------
	hq_uint32 x0,y0,x1,y1;
	hq_float32 t1,t2;
	hq_ubyte8 *c0,*c1,*c2,*c3;


	x0=(hq_uint32)floorf(u);
	y0=(hq_uint32)floorf(v);

	x1=x0+1;
	y1=y0+1;

	t1=(u-x0);//hệ số nội suy ngang
	t2=(v-y0);//hệ số nội suy dọc

	bool outOfWidth=(x1>=width);
	bool outOfHeight=(y1>=height);
	
	//giá trị pixel ở (x0,y0)
	c0=pData+lineSize*y0+x0*4;
	//giá trị pixel ở (x1,y0)
	if(!outOfWidth)
		c1=pData+lineSize*y0+x1*4;
	else c1=c0;
	//giá trị pixel ở (x0,y1)
	if(!outOfHeight)
		c2=pData+lineSize*y1+x0*4;
	else c2=c0;
	//giá trị pixel ở (x1,y1)
	if(outOfHeight)
		c3=c1;
	else if(outOfWidth)
		c3=c2;
	else
		c3=pData+lineSize*y1+x1*4;
	
	hq_float32 A,B;
	//kênh màu thứ 1
	A=Lerp(c0[0]/255.0f,c1[0]/255.0f,t1);
	B=Lerp(c2[0]/255.0f,c3[0]/255.0f,t1);
	
	pPixel[0]=(hq_ubyte8)(255*Lerp(A,B,t2));

	//kênh màu thứ 2
	A=Lerp(c0[1]/255.0f,c1[1]/255.0f,t1);
	B=Lerp(c2[1]/255.0f,c3[1]/255.0f,t1);
	
	pPixel[1]=(hq_ubyte8)(255*Lerp(A,B,t2));

	//kênh màu thứ 3
	A=Lerp(c0[2]/255.0f,c1[2]/255.0f,t1);
	B=Lerp(c2[2]/255.0f,c3[2]/255.0f,t1);
	
	pPixel[2]=(hq_ubyte8)(255*Lerp(A,B,t2));

	//kênh màu thứ 4
	A=Lerp(c0[3]/255.0f,c1[3]/255.0f,t1);
	B=Lerp(c2[3]/255.0f,c3[3]/255.0f,t1);
	
	pPixel[3]=(hq_ubyte8)(255*Lerp(A,B,t2));

}


void Bitmap::BilinearFiler16AL(hq_ubyte8 *pPixel, hq_float32 u, hq_float32 v,const hq_uint32 lineSize)
{
	//----------------------
	//c0(x0,y0)------A------------ c1(x1,y0)
	//		|		 |				|
	//		|--------p(u,v)			|
	//		|		 |				|
	//		|		 |				|
	//c2(x0,y1)------B------------ c3(x1,y1)
	//----------------------
	hq_uint32 x0,y0,x1,y1;
	hq_float32 t1,t2;
	hq_ubyte8 *c0,*c1,*c2,*c3;


	x0=(hq_uint32)floorf(u);
	y0=(hq_uint32)floorf(v);

	x1=x0+1;
	y1=y0+1;

	t1=(u-x0);//hệ số nội suy ngang
	t2=(v-y0);//hệ số nội suy dọc

	bool outOfWidth=(x1>=width);
	bool outOfHeight=(y1>=height);
	
	//giá trị pixel ở (x0,y0)
	c0=pData+lineSize*y0+x0*2;
	//giá trị pixel ở (x1,y0)
	if(!outOfWidth)
		c1=pData+lineSize*y0+x1*2;
	else c1=c0;
	//giá trị pixel ở (x0,y1)
	if(!outOfHeight)
		c2=pData+lineSize*y1+x0*2;
	else c2=c0;
	//giá trị pixel ở (x1,y1)
	if(outOfHeight)
		c3=c1;
	else if(outOfWidth)
		c3=c2;
	else
		c3=pData+lineSize*y1+x1*2;
	
	hq_float32 A,B;
	//kênh màu thứ 1
	A=Lerp(c0[0]/255.0f,c1[0]/255.0f,t1);
	B=Lerp(c2[0]/255.0f,c3[0]/255.0f,t1);
	
	pPixel[0]=(hq_ubyte8)(255*Lerp(A,B,t2));

	//kênh màu thứ 2
	A=Lerp(c0[1]/255.0f,c1[1]/255.0f,t1);
	B=Lerp(c2[1]/255.0f,c3[1]/255.0f,t1);
	
	pPixel[1]=(hq_ubyte8)(255*Lerp(A,B,t2));

}


void Bitmap::BilinearFiler16RGB(hq_ubyte8 *pPixel, hq_float32 u, hq_float32 v,const hq_uint32 lineSize)
{
	//----------------------
	//c0(x0,y0)------A------------ c1(x1,y0)
	//		|		 |				|
	//		|--------p(u,v)			|
	//		|		 |				|
	//		|		 |				|
	//c2(x0,y1)------B------------ c3(x1,y1)
	//----------------------
	hq_uint32 x0,y0,x1,y1;
	hq_float32 t1,t2;
	hq_ushort16 *c0,*c1,*c2,*c3;//16 bit color 


	x0=(hq_uint32)floorf(u);
	y0=(hq_uint32)floorf(v);

	x1=x0+1;
	y1=y0+1;

	t1=(u-x0);//hệ số nội suy ngang
	t2=(v-y0);//hệ số nội suy dọc

	bool outOfWidth=(x1>=width);
	bool outOfHeight=(y1>=height);
	
	//giá trị pixel ở (x0,y0)
	c0=(hq_ushort16 *)(pData+lineSize*y0+x0*2);
	//giá trị pixel ở (x1,y0)
	if(!outOfWidth)
		c1=(hq_ushort16 *)(pData+lineSize*y0+x1*2);
	else c1=c0;
	//giá trị pixel ở (x0,y1)
	if(!outOfHeight)
		c2=(hq_ushort16 *)(pData+lineSize*y1+x0*2);
	else c2=c0;
	//giá trị pixel ở (x1,y1)
	if(outOfHeight)
		c3=c1;
	else if(outOfWidth)
		c3=c2;
	else
		c3=(hq_ushort16 *)(pData+lineSize*y1+x1*2);
	
	hq_float32 R[2],G[2],B[2];
	//điểm A
	R[0]=Lerp(GetRfromRGB16(*c0)/31.0f,GetRfromRGB16(*c1)/31.0f,t1);
	G[0]=Lerp(GetGfromRGB16(*c0)/63.0f,GetGfromRGB16(*c1)/63.0f,t1);
	B[0]=Lerp(GetBfromRGB16(*c0)/31.0f,GetBfromRGB16(*c1)/31.0f,t1);

	//điểm B
	R[1]=Lerp(GetRfromRGB16(*c2)/31.0f,GetRfromRGB16(*c3)/31.0f,t1);
	G[1]=Lerp(GetGfromRGB16(*c2)/63.0f,GetGfromRGB16(*c3)/63.0f,t1);
	B[1]=Lerp(GetBfromRGB16(*c2)/31.0f,GetBfromRGB16(*c3)/31.0f,t1);
	
	*((hq_ushort16 *)pPixel)=RGB16((hq_uint32)(Lerp(R[0],R[1],t2)*31.0f),
									  (hq_uint32)(Lerp(G[0],G[1],t2)*63.0f),
									  (hq_uint32)(Lerp(B[0],B[1],t2)*31.0f));

}

void Bitmap::BilinearFiler8(hq_ubyte8 *pPixel, hq_float32 u, hq_float32 v,const hq_uint32 lineSize)
{
	//----------------------
	//c0(x0,y0)------A------------ c1(x1,y0)
	//		|		 |				|
	//		|--------p(u,v)			|
	//		|		 |				|
	//		|		 |				|
	//c2(x0,y1)------B------------ c3(x1,y1)
	//----------------------
	hq_uint32 x0,y0,x1,y1;
	hq_float32 t1,t2;
	hq_ubyte8 *c0,*c1,*c2,*c3;


	x0=(hq_uint32)floorf(u);
	y0=(hq_uint32)floorf(v);

	x1=x0+1;
	y1=y0+1;

	t1=(u-x0);//hệ số nội suy ngang
	t2=(v-y0);//hệ số nội suy dọc

	bool outOfWidth=(x1>=width);
	bool outOfHeight=(y1>=height);
	
	//giá trị pixel ở (x0,y0)
	c0=pData+lineSize*y0+x0;
	//giá trị pixel ở (x1,y0)
	if(!outOfWidth)
		c1=pData+lineSize*y0+x1;
	else c1=c0;
	//giá trị pixel ở (x0,y1)
	if(!outOfHeight)
		c2=pData+lineSize*y1+x0;
	else c2=c0;
	//giá trị pixel ở (x1,y1)
	if(outOfHeight)
		c3=c1;
	else if(outOfWidth)
		c3=c2;
	else
		c3=pData+lineSize*y1+x1;
	
	hq_float32 A,B;
	//kênh màu thứ 1
	A=Lerp(c0[0]/255.0f,c1[0]/255.0f,t1);
	B=Lerp(c2[0]/255.0f,c3[0]/255.0f,t1);
	
	pPixel[0]=(hq_ubyte8)(255*Lerp(A,B,t2));

}

bool Bitmap::IsCompressed() const
{
	return format == FMT_S3TC_DXT1 || format == FMT_S3TC_DXT3 || format == FMT_S3TC_DXT5 ||
		format == FMT_ETC1 || format == FMT_PVRTC_RGB_2BPP ||
		format == FMT_PVRTC_RGB_4BPP || format == FMT_PVRTC_RGBA_2BPP ||
		format == FMT_PVRTC_RGBA_4BPP;
}

bool Bitmap::IsPVRTC() const
{
	return format == FMT_PVRTC_RGB_2BPP ||
		format == FMT_PVRTC_RGB_4BPP || format == FMT_PVRTC_RGBA_2BPP ||
		format == FMT_PVRTC_RGBA_4BPP;
}

int Bitmap::DeCompress(bool flipRGB)
{
	switch (format)
	{
	case FMT_S3TC_DXT1 : case FMT_S3TC_DXT3 : case FMT_S3TC_DXT5:
		return DeCompressDXT(flipRGB);
	case FMT_ETC1:
		return DeCompressETC(flipRGB);
	default:
		return IMG_FAIL_NOT_SUPPORTED;
	}
}

//giải nén DXT
int Bitmap::DeCompressDXT(bool flipRGB)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
		return IMG_FAIL_NOT_SUPPORTED;
	
	hquint32 faces = 1;
	if (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE)
		faces = 6;

	hq_uint32 nMip=(complex.nMipMap>0)? complex.nMipMap : 1;//số mipmap level
	hq_ushort16 pixelSize;
	if(format==FMT_S3TC_DXT1 || format==FMT_S3TC_DXT3 || format==FMT_S3TC_DXT5)
		pixelSize=4;//4 bytes for A8R8G8B8 format
	else 
		return IMG_FAIL_NOT_SUPPORTED;

	hq_uint32 *sizeOfLevel=new hq_uint32[nMip];//size của từng mipmap level
	
	hq_uint32 w=width;//chiều rộng mipmap level 1
	hq_uint32 h=height;//chiều cao mipmap level 1
	//size của dữ liệu giải nén
	hq_uint32 newSize=0;
	//tính toán size cho dữ liệu giải nén
	for(hq_uint32 mipLevel=0;mipLevel<nMip;++mipLevel)
	{
		if(w==0)
			w=1;
		if(h==0)
			h=1;
		sizeOfLevel[mipLevel]=w*h*pixelSize;
		newSize +=sizeOfLevel[mipLevel];//thêm độ lớn của mipmap level này
		h >>=1;
		w >>=1;
	}
	newSize *= faces;

	hq_ubyte8*newData=new hq_ubyte8[newSize];
	if(!newData)
	{
		delete[] sizeOfLevel;
		return IMG_FAIL_MEM_ALLOC;
	}

	
	hq_ubyte8 *pBlocks=pData;//data chưa giải nén
	hq_ubyte8 *pCur=newData;//data chứa dữ liệu sẽ được giải nén
	for (hquint32 face = 0 ; face < faces ; ++face)
	{
		w=width;h=height;//level 0
		for(hq_uint32 mipLevel=0;mipLevel<nMip;++mipLevel)
		{
			if(w==0)
				w=1;
			if(h==0)
				h=1;
			hq_uint32 size;//size của 1 mipmap level chưa giải nén
			switch(format)
			{
			case FMT_S3TC_DXT1:
				size= ((w+3)/4)*((w+3)/4) *8;
				DecodeDXT1(pBlocks,pCur,w,h , flipRGB);
				break;
			case FMT_S3TC_DXT3:
				size= ((w+3)/4)*((w+3)/4) *16;
				DecodeDXT3(pBlocks,pCur,w,h , flipRGB);
				break;
			case FMT_S3TC_DXT5:
				size= ((w+3)/4)*((w+3)/4) *16;
				DecodeDXT5(pBlocks,pCur,w,h , flipRGB);
				break;
			}
			pCur+=sizeOfLevel[mipLevel];//next level
			pBlocks+=size;

			w >>=1;
			h >>=1;
		}
	}

	imgSize=newSize;
	format=flipRGB ? FMT_A8B8G8R8 :FMT_A8R8G8B8;
	bits = 32;
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	delete[] sizeOfLevel;
	pData=newData;
	return IMG_OK;
}

//giải nén ETC
int Bitmap::DeCompressETC(bool flipRGB)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME)
		return IMG_FAIL_NOT_SUPPORTED;
	
	hquint32 faces = 1;
	if (complex.dwComplexFlags & SURFACE_COMPLEX_CUBE)
		faces = 6;

	hq_uint32 nMip=(complex.nMipMap>0)? complex.nMipMap : 1;//số mipmap level
	hq_ushort16 pixelSize;
	if(format==FMT_ETC1)
		pixelSize=3;//3 bytes for R8G8B8 format
	else 
		return IMG_FAIL_NOT_SUPPORTED;

	hq_uint32 *sizeOfLevel=new hq_uint32[nMip];//size của từng mipmap level
	
	hq_uint32 w=width;//chiều rộng mipmap level 1
	hq_uint32 h=height;//chiều cao mipmap level 1
	//size của dữ liệu giải nén
	hq_uint32 newSize=0;
	//tính toán size cho dữ liệu giải nén
	for(hq_uint32 mipLevel=0;mipLevel<nMip;++mipLevel)
	{
		if(w==0)
			w=1;
		if(h==0)
			h=1;
		sizeOfLevel[mipLevel]=w*h*pixelSize;
		newSize +=sizeOfLevel[mipLevel];//thêm độ lớn của mipmap level này
		h >>=1;
		w >>=1;
	}
	newSize *= faces;
	
	hq_ubyte8* newData;
	try{
		newData=new hq_ubyte8[newSize];
	}
	catch (std::bad_alloc e)
	{
		delete[] sizeOfLevel;
		return IMG_FAIL_MEM_ALLOC;
	}

	
	hq_ubyte8 *pBlocks=pData;//data chưa giải nén
	hq_ubyte8 *pCur=newData;//data chứa dữ liệu sẽ được giải nén
	for (hquint32 face = 0 ; face < faces ; ++face)
	{
		w=width;h=height;//level 0
		for(hq_uint32 mipLevel=0;mipLevel<nMip;++mipLevel)
		{
			if(w==0)
				w=1;
			if(h==0)
				h=1;
			hq_uint32 size;//size của 1 mipmap level chưa giải nén
			switch(format)
			{
			case FMT_ETC1:
				size= ((w+3)/4)*((w+3)/4) *8;
				DecodeETC1(pBlocks,pCur,w,h , flipRGB);
				break;
			}
			pCur+=sizeOfLevel[mipLevel];//next level
			pBlocks+=size;

			w >>=1;
			h >>=1;
		}
	}

	imgSize=newSize;
	format=flipRGB ? FMT_B8G8R8 :FMT_R8G8B8;
	bits = 24;
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	delete[] sizeOfLevel;
	pData=newData;
	return IMG_OK;
}

int Bitmap::RGB16ToRGB24(bool flipRGB)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_R5G6B5 && format != FMT_B5G6R5)
		return IMG_FAIL_NOT_SUPPORTED;

	int redChannel = flipRGB? 0 : 2;
	int blueChannel = 2 - redChannel;

	hq_uint32 newSize=imgSize * 3 / 2;//16 bit => 24 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;pCur+=2,pPixel+=3)
	{
		hq_ushort16 pixel=*((hq_ushort16*)pCur);
		//blue
		pPixel[blueChannel]=(hq_ubyte8)(GetBfromRGB16(pixel) / 31.f * 255.f);   
		//green
		pPixel[1]=(hq_ubyte8)(GetGfromRGB16(pixel) / 63.f * 255.f);     
		//red
		pPixel[redChannel]=(hq_ubyte8)(GetRfromRGB16(pixel) / 31.f * 255.f) ;
	}

	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=24;

	if (format == FMT_R5G6B5)
		format=flipRGB? FMT_B8G8R8 : FMT_R8G8B8;
	else
		format=flipRGB? FMT_R8G8B8 : FMT_B8G8R8;

	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::RGB16ToRGBA(bool flipRGB)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_R5G6B5 && format != FMT_B5G6R5)
		return IMG_FAIL_NOT_SUPPORTED;

	int redChannel = flipRGB? 0 : 2;
	int blueChannel = 2 - redChannel;

	hq_uint32 newSize=imgSize*2;//16 bit => 32 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;pCur+=2,pPixel+=4)
	{
		hq_ushort16 pixel=*((hq_ushort16*)pCur);
		//blue
		pPixel[blueChannel]=(hq_ubyte8)(GetBfromRGB16(pixel) / 31.f * 255.f) ;   
		//green
		pPixel[1]=(hq_ubyte8)(GetGfromRGB16(pixel) / 63.f * 255.f) ;     
		//red
		pPixel[redChannel]=(hq_ubyte8)(GetRfromRGB16(pixel) / 31.f * 255.f);
		//alpha
		pPixel[3]=0xff;//255
	}

	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=32;

	if (format == FMT_R5G6B5)
		format=flipRGB? FMT_A8B8G8R8 : FMT_A8R8G8B8;
	else
		format=flipRGB? FMT_A8R8G8B8 : FMT_A8B8G8R8;

	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::L8ToAL16()
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_L8)
		return IMG_FAIL_NOT_SUPPORTED;

	hq_uint32 newSize=imgSize*2;//8 bit => 16 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;++pCur,pPixel+=2)
	{
		//luminance
		pPixel[0]=*pCur;
		//alpha
		pPixel[1]=0xff;//255
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=16;
	format=FMT_A8L8;
	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::L8ToRGB(bool flipRGB)//chuyển dạng 8 bit greyscale thành R8G8B8
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_L8)
		return IMG_FAIL_NOT_SUPPORTED;
	
	int redChannel = flipRGB? 0 : 2;
	int blueChannel = 2 - redChannel;

	hq_uint32 newSize=imgSize*3;//8 bit => 24 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;++pCur,pPixel+=3)
	{
		//blue
		pPixel[blueChannel]=*pCur;
		//green
		pPixel[1]=*pCur;
		//red
		pPixel[redChannel]=*pCur;
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=24;
	format=flipRGB? FMT_B8G8R8 : FMT_R8G8B8;
	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::L8ToRGBA(bool flipRGB)//chuyển dạng 8 bit greyscale thành A8R8G8B8
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_L8)
		return IMG_FAIL_NOT_SUPPORTED;
	
	int redChannel = flipRGB? 0 : 2;
	int blueChannel = 2 - redChannel;

	hq_uint32 newSize=imgSize*4;//8 bit => 32 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;++pCur,pPixel += 4)
	{
		//blue
		pPixel[blueChannel]=*pCur;
		//green
		pPixel[1]=*pCur;
		//red
		pPixel[redChannel]=*pCur;
		//alpha
		pPixel[3] = 0xff;
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=32;
	format=flipRGB? FMT_A8B8G8R8 : FMT_A8R8G8B8;
	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::A8ToRGBA(bool flipRGB)//chuyển dạng 8 bit alpha thành A8R8G8B8
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_A8)
		return IMG_FAIL_NOT_SUPPORTED;

	hq_uint32 newSize=imgSize*4;//8 bit => 32 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;++pCur,pPixel += 4)
	{
		//blue
		pPixel[0] = 0xff;
		//green
		pPixel[1] = 0xff;
		//red
		pPixel[2] = 0xff;
		//alpha
		pPixel[3] = *pCur;
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=32;
	format=flipRGB? FMT_A8B8G8R8 : FMT_A8R8G8B8;
	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::AL16ToRGBA(bool flipRGB)//chuyển dạng 8 bit greyscale và 8 bit alpha thành A8R8G8B8
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_A8L8)
		return IMG_FAIL_NOT_SUPPORTED;
	
	int redChannel = flipRGB? 0 : 2;
	int blueChannel = 2 - redChannel;

	hq_uint32 newSize=imgSize*2;//16 bit => 32 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd; pCur +=2 ,pPixel+=4)
	{
		//blue
		pPixel[blueChannel]=pCur[0];
		//green
		pPixel[1]=pCur[0];
		//red
		pPixel[redChannel]=pCur[0];
		//alpha
		pPixel[3]=pCur[1];
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=32;
	format= flipRGB? FMT_A8B8G8R8 : FMT_A8R8G8B8;
	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::RGB24ToRGBA(bool flipRGB)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_R8G8B8 && format != FMT_B8G8R8)
		return IMG_FAIL_NOT_SUPPORTED;
	
	int thirdChannel = flipRGB? 0 : 2;
	int firstChannel = 2 - thirdChannel;

	hq_uint32 newSize=(imgSize/3)* 4;//24 bit => 32 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;pCur+=3,pPixel+=4)
	{
		//RGB
		pPixel[firstChannel]=pCur[0];
		pPixel[1]=pCur[1];
		pPixel[thirdChannel]=pCur[2];
		//alpha
		pPixel[3]=0xff;//255
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=32;
	if (format == FMT_R8G8B8)
		format=flipRGB? FMT_A8B8G8R8 : FMT_A8R8G8B8;
	else
		format = flipRGB? FMT_A8R8G8B8 : FMT_A8B8G8R8;
	imgSize=newSize;
	return IMG_OK;
}

int Bitmap::RGBAToRGB24(bool flipRGB)
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;
	if(format!=FMT_A8R8G8B8 && format != FMT_A8B8G8R8 && 
		format != FMT_X8B8G8R8 && format != FMT_X8B8G8R8)
		return IMG_FAIL_NOT_SUPPORTED;
	
	int thirdChannel = flipRGB? 0 : 2;
	int firstChannel = 2 - thirdChannel;

	hq_uint32 newSize=(imgSize/4)* 3;//32 bit => 24 bit
	hq_ubyte8* newData=new hq_ubyte8[newSize];

	if(!newData)
		return IMG_FAIL_MEM_ALLOC;
	hq_ubyte8 *pEnd=pData+imgSize;
	hq_ubyte8 *pCur=pData;
	hq_ubyte8 *pPixel=newData;

	for (;pCur<pEnd;pCur+=4,pPixel+=3)
	{
		//RGB
		pPixel[firstChannel]=pCur[0];
		pPixel[1]=pCur[1];
		pPixel[thirdChannel]=pCur[2];
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	pData=newData;
	bits=24;
	if (format == FMT_A8R8G8B8 || format == FMT_X8R8G8B8)
		format=flipRGB? FMT_B8G8R8 : FMT_R8G8B8;
	else
		format = flipRGB? FMT_R8G8B8 : FMT_B8G8R8;
	imgSize=newSize;
	return IMG_OK;
}


//truy vấn độ lớn của của dữ liệu ảnh hoặc level đầu mipmap nếu có
hq_uint32 Bitmap::GetFirstLevelSize()  const
{
	return this->CalculateSize(this->width, this->height);
}
hq_uint32 Bitmap::CalculateSize(hq_uint32 _width, hq_uint32 _height) const
{
	switch(format)
	{
	case FMT_S3TC_DXT1:case FMT_ETC1:
		return ((_width+3)/4)*((_height+3)/4)*8;
	case FMT_S3TC_DXT3:case FMT_S3TC_DXT5:
		return ((_width+3)/4)*((_height+3)/4)*16;
	case FMT_PVRTC_RGB_2BPP: case FMT_PVRTC_RGBA_2BPP:
		return ( max(_width, 16) * max(_height, 8) * 2 + 7) / 8;
	case FMT_PVRTC_RGB_4BPP: case FMT_PVRTC_RGBA_4BPP:
		return ( max(_width, 8) * max(_height, 8) * 4 + 7) / 8;
	default:
		return _width*_height*(bits/8);
	}
}
hq_uint32 Bitmap::CalculateRowSize(hq_uint32 _width) const
{
	switch(format)
	{
	case FMT_S3TC_DXT1:case FMT_ETC1:
		return ((_width+3)/4)*8;
	case FMT_S3TC_DXT3:case FMT_S3TC_DXT5:
		return ((_width+3)/4)*16;
	case FMT_PVRTC_RGB_2BPP: case FMT_PVRTC_RGBA_2BPP:
		return ( max(_width, 16) * 16 + 7) / 8;
	case FMT_PVRTC_RGB_4BPP: case FMT_PVRTC_RGBA_4BPP:
		return ( max(_width, 8) * 32 + 7) / 8;
	default:
		return _width*(bits/8);
	}
}

int Bitmap::GenerateMipmaps()
{
	if(!pData)
		return IMG_FAIL_FILE_NOT_EXIST;

	if(complex.dwComplexFlags & SURFACE_COMPLEX_CUBE ||
		complex.dwComplexFlags & SURFACE_COMPLEX_VOLUME ||
		complex.dwComplexFlags & SURFACE_COMPLEX_MIPMAP ||
		format == FMT_S3TC_DXT1 || format == FMT_S3TC_DXT3 || 
		format == FMT_S3TC_DXT5 || format == FMT_UNKNOWN)
		return IMG_FAIL_NOT_SUPPORTED;//not supported yet

	//caculate new size
	hq_uint32 newSize = 0;
	hq_uint32 w = this->width;
	hq_uint32 h = this->height;
	
	bool lastLevel = false;
	while(!lastLevel)
	{
		if (w == 1 && h == 1)
			lastLevel = true;
		newSize += this->CalculateSize(w , h);
		if(w > 1) w >>= 1;//w /= 2
		if(h > 1) h >>= 1;//h /= 2
	}
	
	hq_ubyte8 *pNewData = NULL;
	pNewData = HQ_NEW hq_ubyte8[newSize];

	if(pNewData == NULL)
		return IMG_FAIL_MEM_ALLOC;
	
	
	/*----generate mipmaps--------*/
	hq_uint32 currentW = this->width;
	hq_uint32 currentH = this->height;
	hq_ubyte8 *pCur = pNewData;
	hq_uint32 lvlSize = this->imgSize;
	w = this->width;
	h = this->height;
	lastLevel = false;
	this->complex.nMipMap = 0;
	while(!lastLevel)
	{
		if (w == 1 && h == 1)
			lastLevel = true;
		memcpy(pCur, pData , lvlSize);
		pCur += lvlSize;
		lvlSize /= 4;

		if(w > 1) w >>= 1;//w /= 2
		if(h > 1) h >>= 1;//h /= 2

		this->Scalei(w , h);

		this->complex.nMipMap ++;
	}
	
	ftype=IMG_UNKNOWN;
	
	DeletePixelData();
	this->complex.dwComplexFlags |= SURFACE_COMPLEX_MIPMAP;
	this->pData = pNewData;
	this->imgSize = newSize;
	this->width = currentW;
	this->height = currentH;
	return IMG_OK;
}

//Xem thông tin về error code
void Bitmap::GetErrorDesc(int errCode, char *buffer)
{
	switch (errCode)
	{
	case IMG_FAIL_BAD_FORMAT:
		strcpy(buffer,"bad format!");
		return;
	case IMG_FAIL_NOT_ENOUGH_CUBE_FACES:
		strcpy(buffer,"not enough cube faces!");
		return;
	case IMG_FAIL_NOT_SUPPORTED:
		strcpy(buffer,"it's not supported!");
		return;
	case IMG_FAIL_FILE_NOT_EXIST:
		strcpy(buffer,"file doesn't exist!");
		return;
	case IMG_FAIL_MEM_ALLOC:
		strcpy(buffer,"memory allocation failed!");
		return;
	case IMG_FAIL_CUBE_MAP_LOAD_NOT_SAME_FORMAT:
		strcpy(buffer,"cube map faces can't be loaded from different pixel format images!");
		return;
	default:
		sprintf(buffer,"unknown code :%d",errCode);
	}
}

//loader helper function
void Bitmap::GetPixelDataFromStream(hqubyte8* pPixelData, hquint32 totalSize)
{
	switch (this->format)
	{
	case FMT_R8G8B8: case FMT_B8G8R8:case FMT_X8R8G8B8: case FMT_X8B8G8R8:
		if (((this->format == FMT_R8G8B8 || this->format == FMT_X8R8G8B8)&& this->layout == LAYOUT_BGR) ||
			((this->format == FMT_B8G8R8 || this->format == FMT_X8B8G8R8)&& this->layout == LAYOUT_RGB))//need to flip RGB
		{
			size_t pixelSize;
			switch (this->format)
			{
			case FMT_R8G8B8: 
				this->format = FMT_B8G8R8 ; 
				pixelSize = 3;
				break;
			case FMT_X8R8G8B8: 
				this->format = FMT_X8B8G8R8 ; 
				pixelSize = 4;
				break;
			case FMT_B8G8R8: 
				pixelSize = 3;
				this->format = FMT_R8G8B8 ; 
				break;
			case FMT_X8B8G8R8:
				pixelSize = 4;
				this->format = FMT_X8R8G8B8 ; 
				break;
			}
			hqubyte8 pixel[4];
			size_t totalPixels = totalSize / pixelSize;//total pixels
			hqubyte8 *pCur = pPixelData;
			for (size_t i = 0 ; i < totalPixels; ++i)
			{
				stream->GetBytes(pixel, pixelSize);
				pCur[0] = pixel[2];
				pCur[1] = pixel[1];
				pCur[2] = pixel[0];
				pCur += pixelSize;
			}
		}
		else
			stream->GetBytes(pPixelData, totalSize);
		break;
	case FMT_A8R8G8B8: case FMT_A8B8G8R8:
		if ((this->format == FMT_A8R8G8B8 && this->layout == LAYOUT_BGR) ||
			(this->format == FMT_A8B8G8R8 && this->layout == LAYOUT_RGB))//need to flip RGB
		{
			this->format = (this->format == FMT_A8R8G8B8)?  FMT_A8B8G8R8 : FMT_A8R8G8B8;

			hqubyte8 pixel[4];
			size_t totalPixels = totalSize / 4;//total pixels
			hqubyte8 *pCur = pPixelData;
			for (size_t i = 0 ; i < totalPixels; ++i)
			{
				stream->GetBytes(pixel, 4);
				pCur[0] = pixel[2];
				pCur[1] = pixel[1];
				pCur[2] = pixel[0];
				pCur[3] = pixel[3];
				pCur += 4;
			}
		}
		else
			stream->GetBytes(pPixelData, totalSize);
		break;
	case FMT_R8G8B8A8: case FMT_B8G8R8A8:
		if ((this->format == FMT_R8G8B8A8 && this->layout == LAYOUT_BGR) ||
			(this->format == FMT_B8G8R8A8 && this->layout == LAYOUT_RGB))//need to flip RGB
		{
			this->format = (this->format == FMT_R8G8B8A8)?  FMT_B8G8R8A8 : FMT_R8G8B8A8;

			hqubyte8 pixel[4];
			size_t totalPixels = totalSize / 4;//total pixels
			hqubyte8 *pCur = pPixelData;
			for (size_t i = 0 ; i < totalPixels; ++i)
			{
				stream->GetBytes(pixel, 4);
				pCur[1] = pixel[3];
				pCur[2] = pixel[2];
				pCur[3] = pixel[1];
				pCur[0] = pixel[0];//alpha
				pCur += 4;
			}
		}
		else
			stream->GetBytes(pPixelData, totalSize);
		break;
	case FMT_R5G6B5: case FMT_B5G6R5:
		if ((this->format == FMT_R5G6B5 && this->layout16 == LAYOUT_BGR) ||
			(this->format == FMT_B5G6R5 && this->layout16 == LAYOUT_RGB))//need to flip RGB
		{
			this->format = (this->format == FMT_R5G6B5)? FMT_B5G6R5 : FMT_R5G6B5;

			hqushort16 pixel;
			size_t totalPixels = totalSize / 2;//total pixels
			hqubyte8 *pCur = pPixelData;
			for (size_t i = 0 ; i < totalPixels; ++i)
			{
				stream->GetBytes(&pixel, 2);
				*((hqushort16*) pCur) = SwapRGB16(pixel);
				pCur += 2;
			}
		}
		else
			stream->GetBytes(pPixelData, totalSize);
		break;
	default:
		stream->GetBytes(pPixelData, totalSize);
	}
}
