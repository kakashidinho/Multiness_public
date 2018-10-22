/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_ENGINE_CUSTOM_HEAP_INC_H
#define HQ_ENGINE_CUSTOM_HEAP_INC_H

#include "HQEngineCommon.h"

#if defined HQ_WIN_STORE_PLATFORM || defined HQ_WIN_PHONE_PLATFORM
//undefine default definitions
#ifdef HQ_MALLOC
#	undef HQ_MALLOC 
#endif
#ifdef HQ_REALLOC
#	undef HQ_REALLOC
#endif
#ifdef HQ_FREE
#	undef HQ_FREE
#endif


#ifdef __cplusplus
#ifdef HQ_NEW
#	undef HQ_NEW
#endif
#ifdef HQ_DELETE
#	undef HQ_DELETE
#endif
#endif//#ifdef __cplusplus

#if defined _DEBUG || defined DEBUG

#include <stdlib.h>
#ifdef __cplusplus
#include <new>
#endif

#pragma message ("using custom heap")

#ifdef __cplusplus
extern "C" {
#endif

HQENGINE_API void *HQEngineMalloc (size_t size, const char *file, int line);
HQENGINE_API void *HQEngineRealloc (void *ptr, size_t size, const char *file, int line);
HQENGINE_API void HQEngineFree (void *ptr, const char *file, int line);

#ifdef __cplusplus
}
#endif

#ifdef HQ_NO_CUSTOM_HEAP

#define HQ_MALLOC(size) malloc(size)
#define HQ_REALLOC(ptr, size)  realloc(ptr, size)
#define HQ_FREE(ptr) free(ptr)

#else

#define HQ_MALLOC(size)  (HQEngineMalloc(size, __FILE__, __LINE__))
#define HQ_REALLOC(ptr, size)  (HQEngineRealloc(ptr, size, __FILE__, __LINE__))
#define HQ_FREE(ptr) (HQEngineFree(ptr, __FILE__, __LINE__))

#ifdef HQ_CUSTOM_MALLOC
#pragma message ("using custom malloc")
#define malloc(size) HQ_MALLOC(size)
#define realloc(p, size) HQ_REALLOC(p, size)
#define free(p) HQ_FREE(p)
#endif

#ifdef __cplusplus

#ifdef HQ_CUSTOM_NEW_OPERATOR

#pragma message ("warning: using custom new operator from HQEngine, it will cause undefined behaviors if there are other overloading versions of delete")

inline void  _cdecl operator delete (void* p, const char *file, int line)
{
	HQEngineFree(p, file, line);
}

inline void  _cdecl operator delete[] (void* p, const char *file, int line)
{
	HQEngineFree(p, file, line);
}

inline void * _cdecl operator new (size_t cbSize, const char *file, int line)
{
    void *p = HQEngineMalloc(cbSize, file, line); 
	if (p == NULL)
		throw std::bad_alloc();

    return p;

}

inline void * _cdecl operator new[] (size_t cbSize, const char *file, int line)
{
    void *p = HQEngineMalloc(cbSize, file, line); 
	if (p == NULL)
		throw std::bad_alloc();

    return p;

}

inline void  _cdecl operator delete (void* p)
{
	HQEngineFree(p, NULL, 0);
}

inline void  _cdecl operator delete[] (void* p)
{
	HQEngineFree(p, NULL, 0);
}

inline void * _cdecl operator new (size_t cbSize)
{
    void *p = HQEngineMalloc(cbSize, NULL, 0); 
	if (p == NULL)
		throw std::bad_alloc();

    return p;

}

inline void * _cdecl operator new[] (size_t cbSize)
{
    void *p = HQEngineMalloc(cbSize, NULL, 0); 
	if (p == NULL)
		throw std::bad_alloc();

    return p;

}

#define HQ_NEW new(__FILE__, __LINE__)
#define HQ_DELETE delete

#endif//ifdef HQ_CUSTOM_NEW_OPERATOR

#endif//#ifdef __cplusplus

#endif//ifndef HQ_NO_CUSTOM_HEAP


#endif//#if defined _DEBUG || defined DEBUG

#endif//#if defined HQ_WIN_STORE_PLATFORM

#ifndef HQ_MALLOC
#	define HQ_MALLOC malloc
#endif
#ifndef HQ_REALLOC
#	define HQ_REALLOC realloc
#endif
#ifndef HQ_FREE
#	define HQ_FREE free
#endif


#ifdef __cplusplus
#ifndef HQ_NEW
#	define HQ_NEW new
#endif
#ifndef HQ_DELETE
#	define HQ_DELETE delete
#endif
#endif//#ifdef __cplusplus

#endif//HQ_ENGINE_CUSTOM_HEAP_INC_H
