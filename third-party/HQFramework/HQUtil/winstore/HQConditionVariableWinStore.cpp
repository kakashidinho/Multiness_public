/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "../HQUtilPCH.h"
#include "../../HQConditionVariable.h"

#include <new>

#if defined HQ_WIN_STORE_PLATFORM
#	define USE_EVENT 1
#endif

#if USE_EVENT

#include "../../HQAtomic.h"

struct HQWin32CondVarData
{
	HQWin32CondVarData()
		:m_waitCount(0)
	{
	}

	CRITICAL_SECTION m_critSection;
	HANDLE m_autoResetEvent;
	HQAtomic<hquint32> m_waitCount;
};

HQSimpleConditionVar::HQSimpleConditionVar()
{
	m_platformSpecific = HQ_NEW HQWin32CondVarData();

	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	if(InitializeCriticalSectionEx(&data->m_critSection, 0, 0) == 0)
	{
		HQ_DELETE (data);

		throw std::bad_alloc();
	}

	data->m_autoResetEvent = CreateEventEx (NULL, NULL, 0, EVENT_MODIFY_STATE | STANDARD_RIGHTS_ALL);//auto reset event

	if (data->m_autoResetEvent == NULL)
	{
		DeleteCriticalSection(&data->m_critSection);
		HQ_DELETE (data);

		throw std::bad_alloc();
	}
}

HQSimpleConditionVar::~HQSimpleConditionVar()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	DeleteCriticalSection(&data->m_critSection);

	CloseHandle(data->m_autoResetEvent);

	HQ_DELETE (data);
}

bool HQSimpleConditionVar::TryLock()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;
	return TryEnterCriticalSection(&data->m_critSection) != 0;
}

void HQSimpleConditionVar::Lock()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;
	EnterCriticalSection(&data->m_critSection);
}

void HQSimpleConditionVar::Unlock()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;
	LeaveCriticalSection(&data->m_critSection);
}

void HQSimpleConditionVar::Wait()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	data->m_waitCount ++;

	LeaveCriticalSection(&data->m_critSection);//release mutex


	WaitForSingleObjectEx(data->m_autoResetEvent, INFINITE, FALSE);//wait for event

	//re acquire mutex
	EnterCriticalSection(&data->m_critSection);

	data->m_waitCount --;
}

void HQSimpleConditionVar::Signal()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	if ( data->m_waitCount > 0)
		SetEvent(data->m_autoResetEvent);
}

#else //#if USE_EVENT


struct HQWin32CondVarData
{
	CRITICAL_SECTION m_critSection;
	CONDITION_VARIABLE m_condVar;
};

HQSimpleConditionVar::HQSimpleConditionVar()
{
	m_platformSpecific = HQ_NEW HQWin32CondVarData();

	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	if(InitializeCriticalSectionEx(&data->m_critSection, 0, 0) == 0)
	{
		HQ_DELETE (data);

		throw std::bad_alloc();
	}

	InitializeConditionVariable(&data->m_condVar);
}

HQSimpleConditionVar::~HQSimpleConditionVar()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	DeleteCriticalSection(&data->m_critSection);

	HQ_DELETE (data);
}

bool HQSimpleConditionVar::TryLock()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;
	return TryEnterCriticalSection(&data->m_critSection) != 0;
}

void HQSimpleConditionVar::Lock()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;
	EnterCriticalSection(&data->m_critSection);
}

void HQSimpleConditionVar::Unlock()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;
	LeaveCriticalSection(&data->m_critSection);
}

void HQSimpleConditionVar::Wait()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	SleepConditionVariableCS(&data->m_condVar, &data->m_critSection, INFINITE);
}

void HQSimpleConditionVar::Signal()
{
	HQWin32CondVarData *data = (HQWin32CondVarData*)m_platformSpecific;

	WakeConditionVariable(&data->m_condVar);
}

#endif //#if USE_EVENT
