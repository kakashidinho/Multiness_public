/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef IMG_BYTE_STREAM_H
#define IMG_BYTE_STREAM_H

#include "HQPrimitiveDataType.h"
#include "HQDataStream.h"

#include <stdio.h>

class ImgByteStream
{
public:
	ImgByteStream();
	~ImgByteStream();

	void CreateByteStreamFromMemory(const hqubyte8 * memory, size_t size);
	void CreateByteStreamFromStream(HQDataReaderStream *dataStream);

	void Rewind();
	void Clear();
	void Seek(size_t pos);
	void Advance(size_t offset);
	
	bool IsMemoryMode() const {return isMemoryMode;}
	const hqubyte8 * GetMemorySrc() const {return memory;}

	hqint32 GetByte();//return EOF if reach end of stream
	bool GetBytes(void *bytes, size_t maxSize);
	size_t TryGetBytes(void *buffer, size_t elemSize, size_t numElems);//return number of element successfully read
	size_t GetStreamSize() const {return streamSize;}
private:

	bool isMemoryMode;
	size_t streamSize;
	union{
		HQDataReaderStream * dataStream;
		struct{
			const hqubyte* memory;
			size_t iterator;
		};
	};
};

#endif
