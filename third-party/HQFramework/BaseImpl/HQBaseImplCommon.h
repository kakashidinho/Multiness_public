/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef _HQ_COMMON_H_
#define _HQ_COMMON_H_

#include "../HQPlatformDef.h"

/*-------------------------------------------------*/

#if !defined WIN32 && !defined HQ_WIN_STORE_PLATFORM &&  !defined HQ_WIN_PHONE_PLATFORM
#	ifndef DWORD
#		define DWORD hq_uint32
#	endif
#	ifndef WORD
#		define WORD hq_ushort16
#	endif
#	ifndef UINT
#		define UINT hq_uint32
#	endif
#endif

#if _MSC_VER >= 1800
#	define __FINAL__ final
#else
#	define __FINAL__
#endif


#ifndef SafeDelete
#	define SafeDelete(p){if(p != NULL){delete p;p=NULL;}}
#endif
#ifndef SafeDeleteArray
#	define SafeDeleteArray(p){if(p != NULL){delete[] p;p=NULL;}}
#endif

#ifndef SafeDeleteTypeCast
#	define SafeDeleteTypeCast(casting_type, ptr) {if(ptr != NULL) {delete static_cast<casting_type> (ptr); ptr=NULL; } }
#endif

#ifndef SafeRelease
#define SafeRelease(p) {if(p){p->Release();p=0;}}
#endif
#ifndef SafeDelete
#define SafeDelete(p){if(p){delete p;p=0;}}
#endif
#ifndef SafeDeleteArray
#define SafeDeleteArray(p){if(p){delete[] p;p=0;}}
#endif

#endif
