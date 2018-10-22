/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_ENGINE_COMMON_INTERNAL_H
#define HQ_ENGINE_COMMON_INTERNAL_H

#include "../BaseImpl/HQBaseImplCommon.h"
#include "../HQDataStream.h"

#include <string>
#include <string.h>
#include <stdio.h>


//named object
class HQEngineNamedObjImpl: public virtual HQEngineNamedObj{
public:
	HQEngineNamedObjImpl()
		:m_name(NULL)
	{
	}
	HQEngineNamedObjImpl(const char* _name) 
		:m_name(NULL)
	{
		SetName(_name);
	}
	~HQEngineNamedObjImpl()
	{
		if (m_name) delete[] m_name;
	}

	void SetName(const char* _name){
		if (m_name) delete[] m_name;

		size_t len = strlen(_name);
		m_name = new char[len + 1];
		strncpy(m_name, _name, len);
		m_name[len] = '\0';
	}
	virtual const char* GetName() const {return m_name;}
protected:
	char* m_name;
};


namespace HQEngineHelper
{
///
///allocate memory block, must not deallocate the returned pointer explicitly by free() or delete. Throw std::bad_alloc if failed
///
void* GlobalPoolMalloc(size_t size);
///
///release all previously allocated blocks
///
void GlobalPoolReleaseAll();

///
///allocate a string, must not deallocate the returned pointer explicitly by free() or delete.  Throw std::bad_alloc if failed
///
char * GlobalPoolMallocString(const char* s, size_t len);


void InsertFileNameSuffix(std::string & fileName, const std::string& suffix);

//C functions for data stream
void seek_datastream (void* fileHandle, long offset, int origin);
size_t tell_datastream (void* fileHandle);
size_t read_datastream ( void * ptr, size_t size, size_t count, void * stream );

//for easy porting from standard C io
HQENGINE_API long ftell ( HQDataReaderStream * stream );
HQENGINE_API int fseek ( HQDataReaderStream * stream, long int offset, int origin );
HQENGINE_API void rewind( HQDataReaderStream* stream );
HQENGINE_API int fgetc(HQDataReaderStream *stream);
HQENGINE_API size_t fread ( void * ptr, size_t size, size_t count, HQDataReaderStream * stream );
HQENGINE_API int fclose ( HQDataReaderStream * stream );


};//namespace HQEngineHelper


#endif
