/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include <new>
#include "string.h"
#include "Bitmap.h"
#include "ImgByteStream.h"

//gl format
#define GL_ALPHA 0x1906
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_BGR 0x80E0
#define GL_BGRA 0x80E1
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A

//gl type
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364

//gl compressed format
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_ETC1_RGB8_OES    0x8D64
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG  0x8C00
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG  0x8C01
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0x8C02
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG 0x8C03

#define IS_PVRTC(format) (format == FMT_PVRTC_RGB_4BPP || format == FMT_PVRTC_RGB_2BPP || format == FMT_PVRTC_RGBA_4BPP || format == FMT_PVRTC_RGBA_2BPP)


#define SwapUINT32(value) ((value & 0xff) << 16 | (value & 0xff00) << 8 | (value & 0xff0000) >> 8 | (value & 0xff000000) >> 16)


struct KTX_Header
{
	union{
		struct
		{
			hquint32 endianness;
			hquint32 glType;
			hquint32 glTypeSize;
			hquint32 glFormat;
			hquint32 glInternalFormat;
			hquint32 glBaseInternalFormat;
			hquint32 pixelWidth;
			hquint32 pixelHeight;
			hquint32 pixelDepth;
			hquint32 numberOfArrayElements;
			hquint32 numberOfFaces;
			hquint32 numberOfMipmapLevels;
			hquint32 bytesOfKeyValueData;
		};

		hquint32 field[13];
	};
};


struct ImageHierarchyFaceOrZSlice
{
	ImageHierarchyFaceOrZSlice() : pixelDataPtrOffset(0) {}

	hquint32 pixelDataPtrOffset;
};

struct ImageHierarchyArray
{
	ImageHierarchyArray() : facesOrZSlices(NULL) {}
	~ImageHierarchyArray() {if (facesOrZSlices != NULL) delete[] facesOrZSlices;}

	ImageHierarchyFaceOrZSlice *facesOrZSlices;
};

struct ImageHierarchyMipLevel
{
	ImageHierarchyMipLevel() : arrays(NULL), rowSize(0), numRows(0), levelSize(0) {}
	~ImageHierarchyMipLevel() {if (arrays != NULL) delete[] arrays;}

	hquint32 rowSize;//size of 1 row of image's pixels/blocks in this mipmap level
	hquint32 numRows;//number of rows of image's pixels/blocks in this mipmap level
	hquint32 levelSize;//size of this level, this is used for pvrtc compressed image
	
	ImageHierarchyArray *arrays;
};

struct ImageHierarchy
{
	ImageHierarchy() {mipLevels = NULL;}
	~ImageHierarchy() {Clear();}

	void Clear() {if (mipLevels != NULL) {delete[] mipLevels; mipLevels = NULL;}}

	ImageHierarchyMipLevel *mipLevels;
};

//prototype
static hquint32 GetNumRows (hquint32 height, SurfaceFormat format);//get number of rows of pixels/blocks


int Bitmap::LoadKTX()
{
	KTX_Header header;
	stream->GetBytes(&header, sizeof(KTX_Header));
	
	bool swapEndianess = false;
	if (header.endianness == 0x01020304)//opposite endianess
		swapEndianess = true;
	else if (header.endianness != 0x04030201)
		return IMG_FAIL_BAD_FORMAT;

	if (swapEndianess)
	{
		for (int i = 0 ; i < 13 ; ++i)
			header.field[i] = SwapUINT32(header.field[i]);
	}
	
	if (header.numberOfMipmapLevels == 0)
		header.numberOfMipmapLevels = 1;
	if (header.numberOfArrayElements == 0)
		header.numberOfArrayElements = 1;
	if (header.pixelHeight == 0)
		header.pixelHeight = 1;
	if (header.pixelDepth == 0)
		header.pixelDepth = 1 ;

	this->width = header.pixelWidth;
	this->height = header.pixelHeight;

	//check complex
	int result = CheckKTXComplex(header);
	if (result != IMG_OK)
		return result;

	//get image origin
	result = CheckKTXOrigin(header.bytesOfKeyValueData, swapEndianess);
	if (result != IMG_OK)
		return result;
	
	//get surface format
	result = GetKTXFormatAndPixelBits(header);
	if (result != IMG_OK)
		return result;

	//load image data
	return this->LoadKTXData(header);
	
}

bool Bitmap::IsKTX()
{
	hqubyte8 fileIdentifier[] = {
		0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
	};
	hqubyte8 id[12];
	stream->GetBytes(id, 12);

	if (!memcmp(fileIdentifier, id, 12))
		return true;
	return false;
}

int Bitmap::CheckKTXComplex(const KTX_Header &header)
{
	switch (header.numberOfFaces)
	{
	case 1:
		break;
	case 6:
		this->complex.dwComplexFlags |= SURFACE_COMPLEX_CUBE | SURFACE_COMPLEX_FULL_CUBE_FACES;
		break;
	default:
		return IMG_FAIL_NOT_SUPPORTED;
	}

	if (header.numberOfMipmapLevels > 1)
	{
		this->complex.dwComplexFlags |= SURFACE_COMPLEX_MIPMAP;
		this->complex.nMipMap = header.numberOfMipmapLevels;
	}

	if (header.pixelDepth > 1)
	{
		this->complex.dwComplexFlags |= SURFACE_COMPLEX_VOLUME;
		this->complex.vDepth = header.pixelDepth;
	}

	return IMG_OK;
}

int Bitmap::CheckKTXOrigin(hquint32 bytesOfKeyValueData, bool swapEndianess)
{
	//default orientation
	this->origin = ORIGIN_TOP_LEFT;
	
	//key & value pairs

	while (bytesOfKeyValueData > 0)
	{
		
		hquint32 keyAndValueByteSize, bytesPadding;
		stream->GetBytes(&keyAndValueByteSize, 4);
		if (swapEndianess)
			keyAndValueByteSize = SwapUINT32(keyAndValueByteSize);

		bytesPadding = 3 - ((keyAndValueByteSize + 3) % 4);

		bytesOfKeyValueData -= keyAndValueByteSize + bytesPadding + 4;
		
		//find "KTXorientation" key
		if (keyAndValueByteSize < 15)
			stream->Advance(keyAndValueByteSize + bytesPadding);//next key & value
		else
		{
			hqbyte8 orientationKey[15];
			stream->GetBytes(orientationKey, 15);

			if ((!strcmp(orientationKey, "KTXorientation") || !strcmp(orientationKey, "KTXOrientation"))&& keyAndValueByteSize - 15 >= 8)//found
			{
				//get value
				hqbyte8 * value;
				try{
					value = HQ_NEW hqbyte8[keyAndValueByteSize - 15];
				}
				catch (std::bad_alloc e)
				{
					return IMG_FAIL_MEM_ALLOC;
				}

				stream->GetBytes(value, keyAndValueByteSize - 15);

				
				if (!strncmp(value, "S=r,T=d", 7))
					this->origin = ORIGIN_TOP_LEFT;
				else if (!strncmp(value, "S=r,T=u", 7))
					this->origin = ORIGIN_BOTTOM_LEFT;
				else if (!strncmp(value, "S=l,T=d", 7))
					this->origin = ORIGIN_TOP_RIGHT;
				else if (!strncmp(value, "S=l,T=u", 7))
					this->origin = ORIGIN_BOTTOM_RIGHT;

				delete[] value;
				
				//we found it so ignore the rest key & value pairs
				stream->Advance(bytesPadding);
				stream->Advance(bytesOfKeyValueData);
				bytesOfKeyValueData = 0;//break

			}
			else
				stream->Advance(keyAndValueByteSize - 15 + bytesPadding);//next key & value
		}
	}//while (bytesOfKeyValueData > 0)

	return IMG_OK;
}

int Bitmap::GetKTXFormatAndPixelBits(const KTX_Header &header)
{
	switch (header.glFormat)
	{
	case 0://compressed format
		switch (header.glInternalFormat)
		{
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT : case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			this->format = FMT_S3TC_DXT1;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
			this->format = FMT_S3TC_DXT3;
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			this->format = FMT_S3TC_DXT5;
			break;
		case GL_ETC1_RGB8_OES:
			this->format = FMT_ETC1;
			break;
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
			this->format = FMT_PVRTC_RGB_4BPP;
			break;
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
			this->format = FMT_PVRTC_RGB_4BPP;
			break;
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
			this->format = FMT_PVRTC_RGBA_4BPP;
			break;
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
			this->format = FMT_PVRTC_RGBA_2BPP;
			break;
		default:
			return IMG_FAIL_NOT_SUPPORTED;
		}//switch (header.glInternalFormat)
		
		this->bits = 0;

		break;
	case GL_ALPHA:
		if (header.glType != GL_UNSIGNED_BYTE)
			return IMG_FAIL_NOT_SUPPORTED;
		this->format = FMT_A8;
		this->bits = 8;
		break;
	case GL_LUMINANCE:
		if (header.glType != GL_UNSIGNED_BYTE)
			return IMG_FAIL_NOT_SUPPORTED;
		this->format = FMT_L8;
		this->bits = 8;
		break;
	case GL_LUMINANCE_ALPHA:
		if (header.glType != GL_UNSIGNED_BYTE)
			return IMG_FAIL_NOT_SUPPORTED;
		this->format = FMT_A8L8;
		this->bits = 16;
		break;
	case GL_RGBA:
		if (header.glType != GL_UNSIGNED_BYTE)
			return IMG_FAIL_NOT_SUPPORTED;
		this->format = FMT_A8B8G8R8;
		this->bits = 32;
		break;
	case GL_BGRA:
		if (header.glType != GL_UNSIGNED_BYTE)
			return IMG_FAIL_NOT_SUPPORTED;
		this->format = FMT_A8R8G8B8;
		this->bits = 32;
		break;
	case GL_RGB:
		switch(header.glType)
		{
		case GL_UNSIGNED_BYTE:
			this->format = FMT_B8G8R8;
			this->bits = 24;
			break;
		case GL_UNSIGNED_SHORT_5_6_5:
			this->format = FMT_R5G6B5;
			this->bits = 16;
			break;
		case GL_UNSIGNED_SHORT_5_6_5_REV:
			this->format = FMT_B5G6R5;
			this->bits = 16;
			break;
		default:
			return IMG_FAIL_NOT_SUPPORTED;
		}//switch(header.glType)
		break;
	case GL_BGR:
		switch(header.glType)
		{
		case GL_UNSIGNED_BYTE:
			this->format = FMT_R8G8B8;
			this->bits = 24;
			break;
		case GL_UNSIGNED_SHORT_5_6_5:
			this->format = FMT_B5G6R5;
			this->bits = 16;
			break;
		case GL_UNSIGNED_SHORT_5_6_5_REV:
			this->format = FMT_R5G6B5;
			this->bits = 16;
			break;
		default:
			return IMG_FAIL_NOT_SUPPORTED;
		}//switch(header.glType)
		break;
	default:
		return IMG_FAIL_NOT_SUPPORTED;
	}//switch (header.glFormat)

	return IMG_OK;
}

int Bitmap::CreateKTXPixelDataHolder(const KTX_Header &header, ImageHierarchy &imageHierarchy)
{
	bool cube = header.numberOfFaces > 1;
	hquint32 levelOffset = 0;
	hquint32 totalArrayEleSize = 0;//total size of one array element
	hquint32 w = header.pixelWidth;
	hquint32 h = header.pixelHeight;
	
	try{
		imageHierarchy.mipLevels = HQ_NEW ImageHierarchyMipLevel[header.numberOfMipmapLevels];
		


		if (cube)//cube texture
		{
			hquint32 totalFaceSize = 0;//total size of one face
			//pre calculte full mipmap chain size
			for (hquint32 mip = 0 ; mip < header.numberOfMipmapLevels ; ++mip)
			{
				imageHierarchy.mipLevels[mip].levelSize = this->CalculateSize(w, h);
				imageHierarchy.mipLevels[mip].numRows = GetNumRows(h, format);//get number of rows of pixels/blocks
				imageHierarchy.mipLevels[mip].rowSize = this->CalculateRowSize (w);//size of row of pixels/blocks
				
				totalFaceSize += imageHierarchy.mipLevels[mip].levelSize;
				
				w >>= 1;
				h >>= 1;
				if (w == 0) w = 1;
				if (h == 0) h = 1;

			}//for (mip level)

			totalArrayEleSize = totalFaceSize * header.numberOfFaces;

			for (hquint32 mip = 0 ; mip < header.numberOfMipmapLevels ; ++mip)
			{
				imageHierarchy.mipLevels[mip].arrays = HQ_NEW ImageHierarchyArray[header.numberOfArrayElements];
				for (hquint32 ele = 0; ele < header.numberOfArrayElements ; ++ele)
				{
					imageHierarchy.mipLevels[mip].arrays[ele].facesOrZSlices = HQ_NEW ImageHierarchyFaceOrZSlice[header.numberOfFaces];
					for (hquint32 face = 0 ; face < header.numberOfFaces ; ++face)
					{
						imageHierarchy.mipLevels[mip].arrays[ele].facesOrZSlices[face].pixelDataPtrOffset = 
							face * totalFaceSize + ele * totalArrayEleSize + levelOffset;
							
					}//for cube face
				}//for array element

				
				levelOffset += imageHierarchy.mipLevels[mip].levelSize;

			}//for (mip level)
		}//if (cube)
		else //volume texture or 2D texture
		{
			//pre calculte full mipmap chain size
			for (hquint32 mip = 0 ; mip < header.numberOfMipmapLevels ; ++mip)
			{
				imageHierarchy.mipLevels[mip].levelSize = this->CalculateSize(w, h);
				imageHierarchy.mipLevels[mip].numRows = GetNumRows(h, format);//get number of rows of pixels/blocks
				imageHierarchy.mipLevels[mip].rowSize = this->CalculateRowSize (w);//size of row of pixels/blocks
				
				totalArrayEleSize += imageHierarchy.mipLevels[mip].levelSize* 
					header.pixelDepth;
				w >>= 1;
				h >>= 1;
				if (w == 0) w = 1;
				if (h == 0) h = 1;

			}//for (mip level)
			
			hquint32 sliceSize;//size of one slice
			for (hquint32 mip = 0 ; mip < header.numberOfMipmapLevels ; ++mip)
			{
				sliceSize = imageHierarchy.mipLevels[mip].levelSize;
				imageHierarchy.mipLevels[mip].arrays = HQ_NEW ImageHierarchyArray[header.numberOfArrayElements];
				for (hquint32 ele = 0; ele < header.numberOfArrayElements ; ++ele)
				{
					imageHierarchy.mipLevels[mip].arrays[ele].facesOrZSlices = HQ_NEW ImageHierarchyFaceOrZSlice[header.pixelDepth];
					for (hquint32 depth = 0 ; depth < header.pixelDepth ; ++depth)
					{
						imageHierarchy.mipLevels[mip].arrays[ele].facesOrZSlices[depth].pixelDataPtrOffset = 
							depth * sliceSize + ele * totalArrayEleSize + levelOffset;
							
					}//for depth
				}//for array element

				
				levelOffset += sliceSize * 
					header.pixelDepth;

			}//for (mip level)
		}//volume texture
	}
	catch (std::bad_alloc e)
	{
		imageHierarchy.Clear();
	}

	this->imgSize = totalArrayEleSize * header.numberOfArrayElements;//total image size

	try{
		this->pData = HQ_NEW hqubyte8 [this->imgSize];//alloc memory for image data
	}
	catch (std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}

	return IMG_OK;
}


int Bitmap::LoadKTXData(const KTX_Header &header)
{
	ImageHierarchy imageHierarchy;
	int result = CreateKTXPixelDataHolder(header, imageHierarchy);
	if (result != IMG_OK)
		return result;

	hquint32 rowPadding = 0;
	hquint32 numUnits = (header.numberOfFaces > 1) ? header.numberOfFaces : header.pixelDepth;//unit is either face or z slice
	hquint32 imageSize;

	for (hquint32 level = 0; level < header.numberOfMipmapLevels ; ++level)
	{
		stream->GetBytes(&imageSize, 4);
		hquint32 remain = imageHierarchy.mipLevels[level].rowSize % 4;
		if (remain)
			rowPadding = 4 - remain;

		for (hquint32 ele = 0 ; ele < header.numberOfArrayElements ; ++ele)
		{
			for (hquint32 unit = 0 ; unit < numUnits ; ++unit)
			{
				hqubyte8 *pRow = this->pData + imageHierarchy.mipLevels[level].arrays[ele].facesOrZSlices[unit].pixelDataPtrOffset;

				if (IS_PVRTC(this->format))
				{
					this->GetPixelDataFromStream(pRow, imageHierarchy.mipLevels[level].levelSize);
					pRow += imageHierarchy.mipLevels[level].levelSize;
				}
				else
				{
					for (hquint32 row = 0; row < imageHierarchy.mipLevels[level].numRows ; ++row)
					{
						this->GetPixelDataFromStream(pRow, imageHierarchy.mipLevels[level].rowSize);//get row of pixels/blocks
						pRow += imageHierarchy.mipLevels[level].rowSize;
						stream->Advance(rowPadding);//ignore padding bytes
					}//for row
				}
			}//for unit
		}//for ele

	}//for level
	
	

	return IMG_OK;
}


hquint32 GetNumRows (hquint32 height, SurfaceFormat format)
{
	switch(format)
	{
	case FMT_S3TC_DXT1:case FMT_ETC1: case FMT_S3TC_DXT3:case FMT_S3TC_DXT5:
		return (height + 3) / 4;
	default:
		return height;
	}
}
