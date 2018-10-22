/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_MUTEX_H
#define HQ_MUTEX_H

#include "HQUtil.h"

class HQUTIL_API HQMutex
{
public:
	class ScopeLock
	{
	public:
		inline ScopeLock(HQMutex &mutex) : m_mutex(mutex) {m_mutex.Lock();}
		inline ~ScopeLock() {m_mutex.Unlock();}
	private:
		HQMutex &m_mutex;
	};

	HQMutex();
	~HQMutex();
	bool TryLock();
	void Lock();
	void Unlock();
private:
	void *m_platformSpecific;
};

#endif
