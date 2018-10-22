/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "../HQUtilPCH.h"
#include "../../HQMutex.h"

HQMutex::HQMutex()
{
	m_platformSpecific = HQ_NEW CRITICAL_SECTION();
	if (InitializeCriticalSectionEx((CRITICAL_SECTION*)m_platformSpecific, 0, 0) == 0)
		throw std::bad_alloc();
}

HQMutex::~HQMutex()
{
	DeleteCriticalSection((CRITICAL_SECTION*)m_platformSpecific);

	HQ_DELETE ((CRITICAL_SECTION*)m_platformSpecific);
}

bool HQMutex::TryLock()
{
	return TryEnterCriticalSection((CRITICAL_SECTION*)m_platformSpecific) != 0;
}

void HQMutex::Lock()
{
	EnterCriticalSection((CRITICAL_SECTION*)m_platformSpecific);
}

void HQMutex::Unlock()
{
	LeaveCriticalSection((CRITICAL_SECTION*)m_platformSpecific);
}
