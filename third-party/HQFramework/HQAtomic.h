/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_ATOMIC_INCLUDED_H
#define HQ_ATOMIC_INCLUDED_H

#include "HQMutex.h"

/*----generic HQAtomic------------*/
template <class T>
class HQAtomic
{
public:
	HQAtomic();
	HQAtomic(const T& val);

	operator const T () const;
	const T operator = (const T& val);//return old value
private:
	T m_value;
	mutable HQMutex m_mutex;
};

template <class T>
inline HQAtomic<T>::HQAtomic()
{
	HQMutex::ScopeLock sl(m_mutex);
	m_value = T();
}

template <class T>
inline HQAtomic<T>::HQAtomic(const T& val)
{
	HQMutex::ScopeLock sl(m_mutex);
	m_value = val;
}

template <class T>
inline HQAtomic<T>::operator const T () const
{
	T returnVal;
	HQMutex::ScopeLock sl(m_mutex);
	returnVal = m_value;

	return returnVal;
}

template <class T>
inline const T HQAtomic<T>::operator = (const T& val) 
{
	T returnVal;
	HQMutex::ScopeLock sl(m_mutex);
	returnVal = m_value;

	m_value  = val;

	return returnVal;
}

#if defined WIN32 || (defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM)
#	include "HQAtomicWin32.h"
#elif defined HQ_ANDROID_PLATFORM || defined HQ_IPHONE_PLATFORM || defined HQ_MAC_PLATFORM
#	include "HQAtomicGCC.h"
#else

#include "HQMemAlignment.h"

//mutex version
template <class T>
class HQIntegerType;

template<> class HQIntegerType<hqshort16> {};
template<> class HQIntegerType<hqushort16> {};
template<> class HQIntegerType<hqint32> {};
template<> class HQIntegerType<hquint32> {};
template<> class HQIntegerType<hqint64> {};
template<> class HQIntegerType<hquint64> {};

template <class T>
class HQAtomicInteger: public HQIntegerType<T> {

public:
	HQAtomicInteger();//unintialize value
	HQAtomicInteger(const T val);
	HQAtomicInteger(const HQAtomicInteger<T>& a2);

	operator const T () const {return Get();}
	const T operator = (const T& val) {return Assign(val);}//return old value
	const T operator = (const HQAtomicInteger<T>& a2) {return Assign(a2.Get());}//return old value

	void operator += ( T op2) {  AddAssign( op2); }
	void operator |= ( T op2) {  OrAssign( op2); }
	void operator ^= ( T op2) {  XorAssign( op2); }
	void operator &= ( T op2) {  AndAssign( op2); }
	void operator ++ ( ) {  Increment(); }
	void operator -- ( ) {  Decrement(); }
	void operator ++ ( int op2 ) {  Increment(); }//post fix
	void operator -- ( int op2 ) {  Decrement(); }//post fix
protected:
	const T Get() const;//get value
	const T Assign(const T &val);
	void AddAssign(T op2) ;
	void AndAssign(T op2);
	void OrAssign(T op2);
	void XorAssign(T op2);
	void Increment() ;
	void Decrement() ;

	T m_value;
	mutable HQMutex m_mutex;
};

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger()
{
	HQ_ASSERT_ALIGN(this, sizeof(T));

	//unintialize value
}

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger(const T val)
{
	HQ_ASSERT_ALIGN(this, sizeof(T));
	HQMutex::ScopeLock sl(m_mutex);

	m_value = val;
}

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger(const HQAtomicInteger<T>& a2)
{
	HQ_ASSERT_ALIGN(this, sizeof(T));
	
	HQMutex::ScopeLock sl(m_mutex);

	m_value = a2.Get();
}

template <class T>
inline const T HQAtomicInteger<T>::Get() const
{
	register T returnVal = 0;

	HQMutex::ScopeLock sl(m_mutex);
	
	returnVal = m_value;

	return returnVal;
}

template <class T>
inline const T HQAtomicInteger<T>::Assign(const T& newVal)
{
	register T oldVal = 0;
	HQMutex::ScopeLock sl(m_mutex);
	oldVal = m_value;

	m_value = newVal;

	return oldVal;
}

template <class T>
inline void HQAtomicInteger<T>::AddAssign(T op2) 
{
	HQMutex::ScopeLock sl(m_mutex);
	
	m_value += op2;
}

template <class T>
inline void HQAtomicInteger<T>::AndAssign(T op2) 
{
	HQMutex::ScopeLock sl(m_mutex);

	m_value &= op2;
}

template <class T>
inline void HQAtomicInteger<T>::OrAssign(T op2) 
{
	HQMutex::ScopeLock sl(m_mutex);

	m_value |= op2;
}

template <class T>
inline void HQAtomicInteger<T>::XorAssign(T op2) 
{
	HQMutex::ScopeLock sl(m_mutex);

	m_value ^= op2;
}

template <class T>
inline void HQAtomicInteger<T>::Increment()
{
	HQMutex::ScopeLock sl(m_mutex);

	m_value ++;
}

template <class T>
inline void HQAtomicInteger<T>::Decrement()
{
	HQMutex::ScopeLock sl(m_mutex);

	m_value --;
}

/*-----------integer specializations--------------*/
template <> class HQAtomic<hqint32> : public HQAtomicInteger<hqint32> {
	typedef HQAtomicInteger<hqint32 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const hqint32 val): ParentType(val){}
	HQAtomic(const HQAtomic<hqint32>& a2): ParentType(a2) {}
};

template <> class HQAtomic<hquint32> : public HQAtomicInteger<hquint32> {
	typedef HQAtomicInteger<hquint32 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const hquint32 val): ParentType(val){}
	HQAtomic(const HQAtomic<hqint32>& a2): ParentType(a2) {}
};

#endif//#if defined WIN32 || (defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM)

#endif
