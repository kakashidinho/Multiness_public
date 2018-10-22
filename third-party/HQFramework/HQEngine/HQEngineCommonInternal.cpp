/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "HQEnginePCH.h"
#include "HQEngineCommonInternal.h"
#include "../HQDataStream.h"
#include "../HQLinkedList.h"

#include <stdlib.h>

namespace HQEngineHelper
{

static HQLinkedList<void*> g_allocatedBlocks;

//allocate memory block, must not deallocate the returned pointer explicitly by free() or delete
void* GlobalPoolMalloc(size_t size)
{
	void *block = malloc(size);
	if (block == NULL)
		throw std::bad_alloc();
	g_allocatedBlocks.PushBack(block);

	return block;
}
//release all previously allocated blocks
void GlobalPoolReleaseAll()
{
	HQLinkedList<void*>::Iterator ite;
	g_allocatedBlocks.GetIterator(ite);
	for (;!ite.IsAtEnd(); ++ite)
	{
		void * block = *ite;
		free(block);
	}

	g_allocatedBlocks.RemoveAll();
}

//allocate a string, must not deallocate the returned pointer explicitly by free() or delete
char * GlobalPoolMallocString(const char* s, size_t len)
{
	char * newStr = NULL; 
	try{
		newStr = (char*)GlobalPoolMalloc(len + 1);
	} catch (std::bad_alloc & e)
	{
		throw e;
	}

	strncpy(newStr, s, len);
	newStr[len] = '\0';

	return newStr;
}

void InsertFileNameSuffix(std::string & fileName, const std::string& suffix)
{
	size_t pos = fileName.find_last_of('.');
	if (pos == std::string::npos)
		fileName += suffix;
	else
		fileName.insert(pos, suffix);
}

/*------------C functions for data stream-----------------*/
void seek_datastream (void* fileHandle, long offset, int origin)
{
	HQDataReaderStream* stream = (HQDataReaderStream*) fileHandle;
	stream->Seek(offset, (HQDataReaderStream::StreamSeekOrigin)origin);
}

size_t tell_datastream (void* fileHandle)
{
	HQDataReaderStream* stream = (HQDataReaderStream*) fileHandle;
	return (size_t)stream->Tell();
}

size_t read_datastream ( void * ptr, size_t size, size_t count, void * fileHandle )
{
	HQDataReaderStream* stream = (HQDataReaderStream*) fileHandle;
	return stream->ReadBytes(ptr, size, count);
}

/*-------------for easy porting from standard C io---------------*/
int fgetc(HQDataReaderStream *stream)
{
	return stream->GetByte();
}

size_t fread ( void * ptr, size_t size, size_t count, HQDataReaderStream * stream )
{
	return stream->ReadBytes(ptr, size, count);
}
int fclose ( HQDataReaderStream * stream )
{
	if (stream == NULL)
		return 0;
	stream->Release();
	return 0;
}

long ftell ( HQDataReaderStream * stream )
{
	return stream->Tell();
}

int fseek ( HQDataReaderStream * stream, long int offset, int origin )
{
	return stream->Seek(offset, (HQDataReaderStream::StreamSeekOrigin)origin);
}

void rewind( HQDataReaderStream *stream)
{
	stream->Rewind();
}


};//namespace HQEngineHelper