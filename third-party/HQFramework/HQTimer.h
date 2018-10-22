/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_TIMER_H
#define HQ_TIMER_H
#include "HQUtil.h"

#if defined WIN32 || (defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM)/*---windows---*/

#include <windows.h>
typedef LARGE_INTEGER HQTimeCheckPoint;

#elif defined __APPLE__/*----apple------*/

#include <mach/mach_time.h>
#include <time.h>
typedef uint64_t HQTimeCheckPoint;

#elif defined HQ_LINUX_PLATFORM || defined HQ_ANDROID_PLATFORM /*-----linux/android---------*/

#include <time.h>
typedef timespec HQTimeCheckPoint;

#else
#error need implement
#endif

typedef hqfloat32 HQTime;

class HQUTIL_API HQTimer
{
public:
	///get time check point
	static void GetCheckPoint(HQTimeCheckPoint& checkPoint);
	///get elapsed time in seconds between two check points
	static HQTime GetElapsedTime(const HQTimeCheckPoint& point1 , const HQTimeCheckPoint& point2);
	///suspend thread
	static void Sleep(HQTime seconds);
};

#endif
