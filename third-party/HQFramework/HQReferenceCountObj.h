/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_REF_COUNT_OBJECT_H
#define HQ_REF_COUNT_OBJECT_H

#include "HQPrimitiveDataType.h"

#include <assert.h>

template <typename Counter>
class HQReferenceCountObjTemplate {
public:
	HQReferenceCountObjTemplate();

	hquint32 GetRefCount() const {return m_refCount;}
	void AddRef();
	hquint32 Release();
protected:
	virtual ~HQReferenceCountObjTemplate();
private:
	Counter m_refCount;///reference count
};


template <typename Counter>
inline HQReferenceCountObjTemplate<Counter>::HQReferenceCountObjTemplate()
:m_refCount(1)
{
}


template <typename Counter>
inline HQReferenceCountObjTemplate<Counter>::~HQReferenceCountObjTemplate()
{
}


template <typename Counter>
inline void HQReferenceCountObjTemplate<Counter>::AddRef()
{
	m_refCount ++;
}


template <typename Counter>
inline hquint32 HQReferenceCountObjTemplate<Counter>::Release()
{
	assert(m_refCount > 0);
	
	hquint32 count = (--m_refCount);
	if (count == 0)
	{
		delete this;
	}

	return count;
}

typedef HQReferenceCountObjTemplate<hquint32> HQReferenceCountObj;


#endif
