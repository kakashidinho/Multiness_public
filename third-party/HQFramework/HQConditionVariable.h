/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_CONDITION_VARIABLE_H
#define HQ_CONDITION_VARIABLE_H

#include "HQUtil.h"

///
///Simple condition variable object, only allow to wake one thread at a time
///
class HQUTIL_API HQSimpleConditionVar
{
public:
	class ScopeLock
	{
	public:
		inline ScopeLock(HQSimpleConditionVar &var) : m_condVar(var) {m_condVar.Lock();}
		inline ~ScopeLock() {m_condVar.Unlock();}
	private:
		HQSimpleConditionVar &m_condVar;
	};

	HQSimpleConditionVar();
	~HQSimpleConditionVar();

	bool TryLock();//same as mutex
	void Lock();///same as mutex
	void Unlock();///same as mutex

	void Wait();///release the lock and wait until other thread signals using Signal(), the lock will be re-acquired after this method returns. Must call Lock() before calling this method.

	void Signal();//signal so that a waiting thread will be resumed. Should call Lock() before calling this method.

private:
	void *m_platformSpecific;
};


#endif
