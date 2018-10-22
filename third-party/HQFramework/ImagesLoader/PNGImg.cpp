/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "PNGImg.h"
#include "lpng159/png.h"
#include "ImgLoader.h"
#include <new>

static void user_read_data(png_structp png_ptr, png_bytep data,png_size_t length)
{
	ImgByteStream * stream = (ImgByteStream*) png_get_io_ptr(png_ptr);

	if (stream != NULL)
		stream->GetBytes(data, length);
}

PNGImg::PNGImg()
: pStream (NULL) ,
png_ptr (NULL), 
info_ptr (NULL)
{
	
}

PNGImg::~PNGImg()
{
	Clear();
}

void PNGImg::Clear()
{
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	pStream = NULL;
}

int PNGImg::Load(ImgByteStream &stream, hq_ubyte8* & pixelDataOut,
			hq_uint32 &width,hq_uint32 &height,hq_short16 &bits ,
			hq_uint32 &imgSize, SurfaceFormat &format, 
			OutputRGBLayout layout)
{
	if (pStream == NULL || pStream != &stream)
	{
		if (!IsPNG(stream))
			return IMG_FAIL_BAD_FORMAT;
	}
	
	//init libpng objects
	if (png_ptr == NULL)
	{
		png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (png_ptr == NULL)
			return IMG_FAIL_MEM_ALLOC;

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == NULL)
		{
			png_destroy_read_struct(&png_ptr, NULL, NULL);
			return IMG_FAIL_MEM_ALLOC;
		}
	}

	png_set_sig_bytes(png_ptr, 8);
	png_set_read_fn(png_ptr, this->pStream, user_read_data);

	png_read_info(png_ptr,info_ptr);

	int bit_depth;
	int color_type;

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

	if (bit_depth != 8)
		return IMG_FAIL_NOT_SUPPORTED;

	
	switch (color_type)
	{
	case PNG_COLOR_TYPE_GRAY:
		format = FMT_L8;
		bits = 8;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		format = FMT_A8L8;
		bits = 16;
		break;
	case PNG_COLOR_TYPE_RGB:
		if (layout == LAYOUT_RGB)
		{
			format = FMT_R8G8B8;
			png_set_bgr(png_ptr);
		}
		else
			format = FMT_B8G8R8;
		bits = 24;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		if (layout == LAYOUT_RGB)
		{
			format = FMT_A8R8G8B8;
			png_set_bgr(png_ptr);
		}
		else
			format = FMT_A8B8G8R8;
		bits = 32;
		break;
	default:
		return IMG_FAIL_NOT_SUPPORTED;
	}


	hquint32 lineSize = bits / 8 * width;
	imgSize = lineSize * height;
	
	png_bytepp rowPtr;
	try{
		pixelDataOut = HQ_NEW hqubyte8[imgSize];

		rowPtr = HQ_NEW png_bytep[height];
		for (hquint32 h = 0 ; h < height ; ++h)
			rowPtr[h] = pixelDataOut + lineSize * h;

	}
	catch (std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}
	

	png_read_image(png_ptr,  rowPtr);
	
	png_read_end(png_ptr, NULL);

	delete[] rowPtr;


	return IMG_OK;

}

bool PNGImg::IsPNG(ImgByteStream &stream)
{
	bool png = true;	
	//check if this is png
	hqubyte8 header[8];
	stream.GetBytes(header, 8);

	if(png_sig_cmp(header, 0, 8))
	{
		png = false;
	}
	else
	{
		pStream = &stream;
	}
	

	return png;
}
