/*
Copyright (C) 2010-2014  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_BUFFER_DATA_STREAM_H
#define HQ_BUFFER_DATA_STREAM_H

#include "HQDataStream.h"

#include <string.h>
#include <stdio.h>

#ifndef __min__
#define __min__(a, b) ((a < b) ? a : b)
#endif

#ifndef HQ_CLAMP_TO_ZERO
#define HQ_CLAMP_TO_ZERO(a) (((a) < 0)? 0: (a))
#endif

class HQBufferedDataReader: public HQDataReaderStream
{
public:
	HQBufferedDataReader(HQDataReaderStream *stream, size_t bufferSize = 8192)
	{
		this->stream = stream;
		if (this->stream != NULL)
			this->stream->AddRef();

		this->bufferSize = bufferSize;

		this->buffer = new hqubyte8[this->bufferSize];

		this->ResetStream(0);
	}
	~HQBufferedDataReader()
	{
		Close();
		delete[] this->buffer;
	}

	hqint32 GetByte();
	size_t ReadBytes(void* dataOut, size_t elemSize, size_t elemCount);


	hqint32 Close();
	hqint32 Seek(hqint64 offset, StreamSeekOrigin origin);
	void Rewind();
	hqint64 Tell() const;

	size_t TotalSize() const { return (stream != NULL)? stream->TotalSize() : 0; }
	bool Good() const {return Tell() < TotalSize();}

	const char *GetName() const { if (stream != NULL) return stream->GetName(); return NULL; }
private:
	void ResetStream(size_t position);
	size_t ReadBytes(unsigned char *ptr, size_t bytes);
	bool FillBuffer();
	size_t ReadFromStream(unsigned char *ptr, size_t bytes);

	hqubyte8* buffer;
	size_t bufferSize;
	size_t unconsumedBufferLength;
	size_t bufferOffset;
	size_t streamOffset;

	HQDataReaderStream* stream;
};

inline size_t HQBufferedDataReader::ReadFromStream(unsigned char *ptr, size_t bytes)
{
	size_t readBytes = this->stream->ReadBytes(ptr, 1, bytes);

	//increase stream offset from the start
	this->streamOffset += readBytes;

	return readBytes;
}

inline bool HQBufferedDataReader::FillBuffer()
{
	size_t bytesToRead = __min__(this->TotalSize() - this->streamOffset, this->bufferSize);
	if (bytesToRead == 0)//no available bytes
		return false;

	//read to buffer
	size_t readBytes = this->ReadFromStream(this->buffer, bytesToRead);

	this->unconsumedBufferLength = readBytes;
	this->bufferOffset = 0;


	return true;
}

inline size_t HQBufferedDataReader::ReadBytes(void* dataOut, size_t elemSize, size_t elemCount)
{
	size_t bytesRead = ReadBytes((unsigned char*)dataOut, elemSize * elemCount);

	size_t elems = bytesRead / elemSize;

	size_t remain = bytesRead % elemSize;
	//bytesRead is not divisible by elemSize
	if (remain != 0)
	{
		this->Seek(-((long long)remain), SSO_CURRENT);
	}

	return elems;
}

inline size_t HQBufferedDataReader::ReadBytes(unsigned char *dest_buffer, size_t maxBytesToCopy)
{
	unsigned char *ptr = dest_buffer;
	size_t bytesToCopy = maxBytesToCopy;
	if (this->unconsumedBufferLength > 0 && bytesToCopy >= this->unconsumedBufferLength)
	{
		//read remaining buffer data
		memcpy(ptr, this->buffer + this->bufferOffset, this->unconsumedBufferLength);

		bytesToCopy -= this->unconsumedBufferLength;
		ptr += this->unconsumedBufferLength;

		this->unconsumedBufferLength = 0;
		this->bufferOffset = 0;
	}

	if (bytesToCopy >= this->bufferSize)//read directly from stream
	{
		size_t readBytes = this->ReadFromStream(ptr, bytesToCopy);
		bytesToCopy -= readBytes;
		ptr += readBytes;
	}
	else//read to buffer first for faster small data reading
	{
		while (bytesToCopy > 0)
		{
			bool bufferOK = true;
			if (this->unconsumedBufferLength == 0)
			{
				bufferOK = FillBuffer();
			}

			if (bufferOK)
			{
				//calculate the max possible bytes to be copied from buffer 
				size_t maxPossibleBytesToCopy = __min__(this->unconsumedBufferLength, bytesToCopy);

				//copy from buffer
				memcpy(ptr, this->buffer + this->bufferOffset, maxPossibleBytesToCopy);

				//decrease the available bytes from buffer and total bytes that are still required
				this->unconsumedBufferLength -= maxPossibleBytesToCopy;
				bytesToCopy -= maxPossibleBytesToCopy;

				//increase the offset on buffer
				this->bufferOffset += maxPossibleBytesToCopy;

				//next block
				ptr += maxPossibleBytesToCopy;
			}
			else
			{
				bytesToCopy = 0;//break
			}
		}//while (bytesToCopy > 0)
	}

	return ptr - dest_buffer;
}

inline int HQBufferedDataReader::GetByte()
{
	if (this->streamOffset - this->unconsumedBufferLength >= this->TotalSize())
		return EOF;

	char byte;

	ReadBytes((unsigned char *)&byte, 1);

	return byte;
}

inline int HQBufferedDataReader::Seek(long long offset, StreamSeekOrigin origin)
{
	size_t position = 0;
	size_t currentPosition = HQ_CLAMP_TO_ZERO(Tell());

	switch (origin)
	{
	case SSO_BEGIN:
		position = HQ_CLAMP_TO_ZERO(offset);
		break;
	case SSO_CURRENT:
		position = HQ_CLAMP_TO_ZERO(currentPosition + offset);
		break;
	case SSO_END:
		if (offset > 0)
			position = this->TotalSize();
		else
			position = HQ_CLAMP_TO_ZERO(this->TotalSize() + offset);
		break;
	}

	if (position > this->TotalSize())
		position = this->TotalSize();
	if (position != currentPosition)
		ResetStream(position);
	return 0;
}

inline void HQBufferedDataReader::Rewind()
{
	Seek(0, SSO_BEGIN);
}

long inline long HQBufferedDataReader::Tell() const
{
	return this->streamOffset - this->unconsumedBufferLength;
}

inline void HQBufferedDataReader::ResetStream(size_t position)
{
	if (this->stream == NULL)
	{
		return;
	}

	this->stream->Seek(position, SSO_BEGIN);

	this->bufferOffset = 0;

	this->streamOffset = position;

	this->unconsumedBufferLength = 0;

}

inline int HQBufferedDataReader::Close()
{
	if (stream != NULL)
	{
		stream->Release();
		stream = NULL;
	}

	return 0;
}

#endif
