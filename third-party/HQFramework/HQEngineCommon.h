/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_ENGINE_COMMON_H
#define HQ_ENGINE_COMMON_H

#include "HQPlatformDef.h"
#if defined HQ_IPHONE_PLATFORM || defined HQ_ANDROID_PLATFORM || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
#	ifndef _STATIC_RENDERER_LIB_
#		define _STATIC_RENDERER_LIB_
#	endif 
#endif

#ifndef HQENGINE_API
#	ifdef HQ_STATIC_ENGINE
#		define HQENGINE_API
#	else
#		if defined WIN32 || defined HQ_WIN_STORE_PLATFORM || defined HQ_WIN_PHONE_PLATFORM
#			ifdef HQENGINE_EXPORTS
#				define HQENGINE_API __declspec(dllexport)
#			else
#				define HQENGINE_API __declspec(dllimport)
#			endif
#		else
#				define HQENGINE_API __attribute__ ((visibility("default")))
#		endif
#	endif
#endif



//named object interface
class HQEngineNamedObj{
public:
	virtual const char* GetName() const = 0;
protected:
	virtual ~HQEngineNamedObj() {}
};



#endif
