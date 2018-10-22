// HQUtilPCH.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#ifndef _STD_AFX_UTIL_H
#define _STD_AFX_UTIL_H

#include "../HQPlatformDef.h"
#include "../HQEngineCustomHeap.h"

#if defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
#if !(defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM) 
#include "targetver.h"
#endif//#if !(defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM) 

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#endif//#if defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM

// TODO: reference additional headers your program requires here
#include "../HQUtil.h"
#include <new>

#endif