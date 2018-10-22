/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

// HQEnginePCH.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#ifndef HQ_ENGINE_STD_AFX_H
#define HQ_ENGINE_STD_AFX_H

#ifdef WIN32
#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#endif


// TODO: reference additional headers your program requires here
#include "../HQPlatformDef.h"
#include "../HQEngineCustomHeap.h"
#ifdef HQ_MAC_PLATFORM
#include <Cocoa/Cocoa.h>
#endif

#endif
