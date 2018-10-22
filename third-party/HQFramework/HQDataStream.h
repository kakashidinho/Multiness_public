/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_DATA_STREAM_H
#define HQ_DATA_STREAM_H

#include "HQPrimitiveDataType.h"
#include "HQReferenceCountObj.h"

#include <stdlib.h>
#include <stdio.h>

class HQDataReaderStream : public HQReferenceCountObj
{
public:
	enum StreamSeekOrigin
	{
		SSO_BEGIN = 0,
		SSO_CURRENT = 1,
		SSO_END = 2
	};

	virtual hqint32 GetByte()= 0 ;///get next byte, return -1 if error
	virtual size_t ReadBytes(void* dataOut, size_t elemSize, size_t elemCount)= 0 ;///returns number of successfully read elements
	virtual hqint32 Seek(hqint64 offset, StreamSeekOrigin origin)= 0 ;///returns non-zero on error
	virtual void Rewind()= 0 ;
	virtual hqint64 Tell() const= 0 ;///same as ftell. return number of bytes between the current stream position and the beginning

	virtual size_t TotalSize() const = 0 ;
	virtual bool Good() const = 0 ;///stil not reach end of stream
	
	virtual const char *GetName() const = 0;//can be NULL, this can return name of the opened file

	bool GetLine(char * buffer, size_t maxBufferSize);

protected:
	virtual ~HQDataReaderStream() {}
};


inline bool HQDataReaderStream::GetLine(char * buffer, size_t maxBufferSize)
{
	int c = GetByte();
	size_t currentOffset = 0;
	while (c != EOF && c != '\n' && currentOffset < maxBufferSize - 1)
	{
		buffer[currentOffset++] = c;
		c = GetByte();
	}

	buffer[currentOffset] = 0;

	return currentOffset > 0;
}


#endif
