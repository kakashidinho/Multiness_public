/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "ImgByteStream.h"

#if defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
#include "../HQEngine/winstore/HQWinStoreFileSystem.h"

using namespace HQWinStoreFileSystem;

#endif//#if defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM

#include <string.h>

ImgByteStream::ImgByteStream()
: isMemoryMode(false),
streamSize(0),
memory(NULL),
iterator(0)
{
}
ImgByteStream::~ImgByteStream()
{
	Clear();
}

void ImgByteStream::CreateByteStreamFromMemory(const hqubyte8 * memory, size_t size)
{
	Clear();
	this->memory = memory;
	this->streamSize = size;
	this->isMemoryMode = true;
}
void ImgByteStream::CreateByteStreamFromStream(HQDataReaderStream *dataStream)
{
	Clear();
	this->dataStream = dataStream;
	this->streamSize = this->dataStream->TotalSize();
	this->isMemoryMode = false;
}

void ImgByteStream::Rewind()
{
	if (!this->isMemoryMode)
	{
		if (dataStream != NULL)
			dataStream->Rewind();
	}
	else if (memory != NULL)
		iterator = 0;
}

void ImgByteStream::Seek(size_t pos)
{
	if (!this->isMemoryMode)
	{
		if (dataStream != NULL)
			dataStream->Seek(pos, HQDataReaderStream::SSO_BEGIN);
	}
	else if (memory != NULL)
		iterator = pos;
}

void ImgByteStream::Advance(size_t offset)
{
	if (!this->isMemoryMode)
	{
		if (dataStream != NULL)
			dataStream->Seek(offset, HQDataReaderStream::SSO_CURRENT);
	}
	else if (memory != NULL)
	{
		iterator += offset;
		if (iterator >= streamSize)
			iterator = streamSize - 1;
	}
}

hqint32 ImgByteStream::GetByte()
{
	if (!this->isMemoryMode)
	{
		if (dataStream != NULL)
			return dataStream->GetByte();
	}
	else if (memory != NULL)
	{
		if (iterator >= streamSize)
			return EOF;
		return memory[iterator ++];
	}
	return EOF;
}

bool ImgByteStream::GetBytes(void *bytes, size_t size)
{
	if (!this->isMemoryMode)
	{
		if (dataStream != NULL)
		{
			if (dataStream->ReadBytes(bytes, size, 1)!= 1)
				return false;
		}
		else return false;
	}
	else if (memory != NULL)
	{
		if (streamSize - iterator < size)
			return false;
		memcpy(bytes, memory + iterator, size);
		iterator += size;
	}
	else return false;

	return true;
}

size_t ImgByteStream::TryGetBytes(void *buffer, size_t elemSize, size_t numElems)
{
	if (!this->isMemoryMode)
	{
		if (dataStream != NULL)
		{
			return dataStream->ReadBytes(buffer, elemSize, numElems);
		}
		else return 0;
	}
	else if (memory != NULL)
	{
		size_t readElems = 0;

		for (size_t i = 0; i < numElems; ++i)
		{
			if (streamSize - iterator < elemSize)
				break;
			memcpy((hqbyte8*)buffer + i * elemSize, memory + iterator, elemSize);
			iterator += elemSize;

			readElems++;
		}

		return readElems;
	}

	return 0;
}

void ImgByteStream::Clear()
{
	if (!this->isMemoryMode)
	{
		if (dataStream != NULL)
		{
			//cannot close the stream here
			dataStream = NULL;
		}
	}
	else
	{
		this->memory = NULL;
		this->iterator = 0;
	}
	
	this->isMemoryMode = false;
	this->streamSize = 0;

}
