/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_MEM_ALIGNMENT_H
#define HQ_MEM_ALIGNMENT_H

#include <stdlib.h>
#include <new>
#include "HQAssert.h"
#include "HQPrimitiveDataType.h"

#ifdef new
#	error new defined
#endif

#ifdef delete
#	error delete defined
#endif


inline bool Is_Address_Aligned(void * p, size_t bytesAlignment)
{
	return (((unsigned long)p % bytesAlignment) == 0);
}

#ifndef HQ_ASSERT_ALIGN
#	define HQ_ASSERT_ALIGN(p, alignment) HQ_ASSERT(Is_Address_Aligned(p, alignment))
#endif

inline bool Is_Address_16Bytes_Aligned(void * p)
{
	return (((unsigned long)p & 15) == 0);
}

#ifndef HQ_ASSERT_ALIGN16
#	ifdef HQ_NO_NEED_ALIGN16
#		define HQ_ASSERT_ALIGN16(p)
#	else
#		if defined HQ_ANDROID_PLATFORM
#			define HQ_ASSERT_ALIGN16(p) HQ_ASSERT_SOURCE_LINE(Is_Address_16Bytes_Aligned(p), __FILE__, __LINE__)
#		else
#			define HQ_ASSERT_ALIGN16(p) HQ_ASSERT(Is_Address_16Bytes_Aligned(p))
#		endif
#		define HQ_ASSERT_ALIGN16_SOURCE_LINE(p, source, line) HQ_ASSERT_SOURCE_LINE(Is_Address_16Bytes_Aligned(p), source, line)
#	endif
#endif

#ifndef HQ_ALIGN16
#	ifdef HQ_NO_NEED_ALIGN16
#		define HQ_ALIGN16
#	else
#		if defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
#			define HQ_ALIGN16 __declspec(align(16))
#		else
#			define HQ_ALIGN16 __attribute__ ((aligned (16)))
#		endif
#	endif
#endif

#ifndef HQ_ALIGN16_POSTFIX
#	if !defined WIN32 && !(defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM)
#		define HQ_ALIGN16_POSTFIX HQ_ALIGN16
#	else
#		define HQ_ALIGN16_POSTFIX
#	endif
#endif

/*-------------instance of this class will have 16 byte aligned memory address unless HQ_NO_NEED_ALIGN16 is defined---------------*/
#if defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
class HQ_ALIGN16  HQA16ByteObject
#else
class HQA16ByteObject
#endif
{
public:

	inline void* operator new(size_t size);
	inline void* operator new[](size_t size);
	inline void* operator new(size_t size, const char *file, int line);
	inline void* operator new[](size_t size, const char *file, int line);
	inline void* operator new(size_t size , void *ptr) 
	{	HQ_ASSERT_ALIGN16(ptr)   return ptr;}
	inline void* operator new[](size_t size , void *ptr)
	{	HQ_ASSERT_ALIGN16(ptr)   return ptr;}
	
	inline void operator delete(void* p);
	inline void operator delete[](void* p);
	inline void operator delete(void *p  , void *ptr) {} 
	inline void operator delete[](void *p  , void *ptr) {}
	inline void operator delete(void* p, const char *file, int line);
	inline void operator delete[](void* p, const char *file, int line);

#if !(defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM)
}  HQ_ALIGN16 ;
#else
};
#endif


/*----------------------------------------------------------
HQA16ByteStorage , HQA16ByteStoragePtr and HQA16ByteStorageArrayPtr 
can be used when compiler can't auto align variable 's address 
For example when gcc 's __attribute__ ((aligned (16)))
doesn't work properly
---------------------------------------------------------*/
template <size_t len>
class HQA16ByteStorage
{
public:
	void * operator()(void){return (void*)(((long)(storage + 15)) & ~15);}
	const void * operator()(void) const {return (void*)(((long)(storage + 15)) & ~15);}
	operator void *() {return (void*)(((long)(storage + 15)) & ~15);}
	operator const void *() const {return (void*)(((long)(storage + 15)) & ~15);}
protected:
	char storage [15 + len];
};

template <class T>
class HQA16ByteStoragePtr : public HQA16ByteStorage<sizeof(T)>
{
public:
	HQA16ByteStoragePtr()
	{
		void *p = *this;
		ptr = new (p) T();
	};
	HQA16ByteStoragePtr(const T& src)
	{
		void *p = *this;
		ptr = new (p) T(src);
	}
	explicit HQA16ByteStoragePtr(const void* null)//use this constructor if you don't want to call default constructor of T class
	{
		void *p = *this;
		ptr = (T*)p;
	}
	~HQA16ByteStoragePtr()
	{
		ptr->~T();
	}
	
	using HQA16ByteStorage<sizeof(T)>::operator void *;
	using HQA16ByteStorage<sizeof(T)>::operator const void *;

	operator T*() {return ptr;}
	operator const T*() const {return ptr;}

	T *operator->() {return ptr;}
	const T *operator->() const {return ptr;}
	T & operator *() {return *ptr;}
	const T & operator *() const {return *ptr;}

protected:
	T * ptr;
};

template <class T , size_t arraySize>
class HQA16ByteStorageArrayPtr : public HQA16ByteStorage<sizeof(T) * arraySize>
{
public:
	HQA16ByteStorageArrayPtr()
	{
		void * p = *this;
		ptr = (T*)p;
		for (size_t i = 0 ; i < arraySize ; ++i)
			new (ptr + i) T();
	};
	~HQA16ByteStorageArrayPtr()
	{
		for (size_t i = 0 ; i < arraySize ; ++i)
			ptr[i].~T();
	}
	
	operator T*() {return ptr;}
	operator const T*() const {return ptr;}

	T *operator->() {return ptr;}
	const T *operator->() const {return ptr;}
	T & operator *() {return *ptr;}
	const T & operator *() const {return *ptr;}
	T & operator [](hq_int32 index) {return ptr[index];}
	const T & operator [](hq_int32 index) const {return *ptr[index];}
	T & operator [](size_t index) {return ptr[index];}
	const T & operator [](size_t index) const {return *ptr[index];}

protected:
	T * ptr;
};


/*----------------------------------------*/

inline void * HQAligned16Malloc(size_t size)
{
	char *raw = (char*) malloc(size + 16);
	if (raw == NULL)
		return NULL;
	char *p = (char*)(((long)raw + 16) & ~15);
	*(p - 1) = (char)(p - raw);//store offset in previous byte 

	HQ_ASSERT_ALIGN16(p);

	return p;
};

inline void HQAligned16Free(void * p)
{
	if (p == NULL)
		return;
	char offset = *((char*)p - 1);
	char * raw = (char*)p - offset;
	free(raw);
}

//************************************************
//HQA16ByteObject
//************************************************
inline void* HQA16ByteObject::operator new(size_t size){

#ifdef HQ_NO_NEED_ALIGN16
	void *p = malloc(size);
#else
	void * p = HQAligned16Malloc(size);
#endif

	if(!p)
		throw std::bad_alloc();
	return p;
}
inline void* HQA16ByteObject::operator new[](size_t size){
#ifdef HQ_NO_NEED_ALIGN16
	void *p = malloc(size);
#else
	void * p = HQAligned16Malloc(size);
#endif
	if(!p)
		throw std::bad_alloc();
	return p;
}

inline void* HQA16ByteObject::operator new(size_t size, const char *file, int line)
{
#ifdef HQ_NO_NEED_ALIGN16
	void *p = malloc(size);
#else
	void * p = HQAligned16Malloc(size);
#endif
	if(!p)
		throw std::bad_alloc();
	return p;
}

inline void* HQA16ByteObject::operator new[](size_t size, const char *file, int line)
{
#ifdef HQ_NO_NEED_ALIGN16
	void *p = malloc(size);
#else
	void * p = HQAligned16Malloc(size);
#endif
	if(!p)
		throw std::bad_alloc();
	return p;
}

inline void HQA16ByteObject::operator delete(void *p){
#ifdef HQ_NO_NEED_ALIGN16
	free(p);
#else
	HQAligned16Free(p);
#endif
}

inline void HQA16ByteObject::operator delete[](void *p){
#ifdef HQ_NO_NEED_ALIGN16
	free(p);
#else
	HQAligned16Free(p);
#endif
}

inline void HQA16ByteObject::operator delete(void* p, const char *file, int line)
{
#ifdef HQ_NO_NEED_ALIGN16
	free(p);
#else
	HQAligned16Free(p);
#endif
}

inline void HQA16ByteObject::operator delete[](void* p, const char *file, int line)
{
#ifdef HQ_NO_NEED_ALIGN16
	free(p);
#else
	HQAligned16Free(p);
#endif
}

#endif
