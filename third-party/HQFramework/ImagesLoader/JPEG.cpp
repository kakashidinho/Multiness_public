/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "JPEG.h"
#include "jpeg-8d/jpeglib.h"
#include "ImgLoader.h"
#include "string.h"

#include <new>
#include <setjmp.h>

static jmp_buf jmpBuf;

METHODDEF(void)
custom_error_exit (j_common_ptr cinfo)
{
#if defined _DEBUG || defined DEBUG
	/* Always display the message */
	//(*cinfo->err->output_message) (cinfo);
#endif
	/* Let the memory manager delete any temp files before we die */
	//jpeg_destroy(cinfo);
	jpeg_abort_decompress((jpeg_decompress_struct*)cinfo);

	longjmp(jmpBuf, 1);
}


static size_t custom_fread(void *buf, size_t elemSize, size_t numElem, void *stream)
{
	ImgByteStream *byteStream = (ImgByteStream*)stream;

	return byteStream->TryGetBytes(buf, elemSize, numElem);
}

JPEGImg::JPEGImg()
: pStream (NULL)
{
	dobj = HQ_NEW jpeg_decompress_struct;
	jerr = HQ_NEW jpeg_error_mgr;
	dobj->err = jpeg_std_error(jerr);
	jerr->error_exit = custom_error_exit;

	jpeg_create_decompress(dobj);
}

JPEGImg::~JPEGImg()
{
	jpeg_destroy_decompress(dobj);
	delete jerr;
	delete dobj;
}

void JPEGImg::Clear()
{
	jpeg_abort_decompress(dobj);
	pStream = NULL;
}

int JPEGImg::Load(ImgByteStream &stream, hq_ubyte8* & pixelDataOut,
			hq_uint32 &width,hq_uint32 &height,hq_short16 &bits ,
			hq_uint32 &imgSize, bool &isGreyscale)
{
	if (pStream == NULL || pStream != &stream)
	{
		if (!IsJPEG(stream))
			return IMG_FAIL_BAD_FORMAT;
	}

	if (dobj->out_color_space != JCS_RGB && dobj->out_color_space != JCS_GRAYSCALE)
		return IMG_FAIL_NOT_SUPPORTED;


	isGreyscale = dobj->out_color_space == JCS_GRAYSCALE;

	jpeg_start_decompress(dobj);

	if (dobj->output_components != dobj->out_color_components)
		return IMG_FAIL_NOT_SUPPORTED;

	bits = 8 * dobj->output_components;

	width = dobj->output_width;
	height = dobj->output_height;

	hquint32 lineSize = dobj->output_components * width;
	imgSize = lineSize * height;

	try{
		pixelDataOut = HQ_NEW hqubyte8[imgSize];
	}
	catch (std::bad_alloc e)
	{
		return IMG_FAIL_MEM_ALLOC;
	}

	hqubyte8 *pLine = pixelDataOut;

	for (hquint32 line = 0 ; line < height ; ++line)
	{
		jpeg_read_scanlines(dobj, &pLine, 1);
		pLine += lineSize;
	}

	jpeg_finish_decompress(dobj);

	return IMG_OK;

}

bool JPEGImg::IsJPEG(ImgByteStream &stream)
{
	if (stream.IsMemoryMode())
		jpeg_mem_src (dobj, (hqubyte8*)stream.GetMemorySrc(), stream.GetStreamSize());
	else
		jpeg_file_src_custom_read(dobj, &stream, custom_fread);

	bool jpeg = true;
	int re = 0;

	if (setjmp(jmpBuf) == 0)
	{
		re = jpeg_read_header(dobj , TRUE);
	}

	if (re != JPEG_HEADER_OK)
	{
		jpeg = false;
	}
	else
		pStream = &stream;
	

	return jpeg;
}
