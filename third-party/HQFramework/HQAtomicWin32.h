/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_ATOMIC_WIN32_INCLUDED_H
#define HQ_ATOMIC_WIN32_INCLUDED_H

#include "HQAtomic.h"
#include "HQMemAlignment.h"

#include <windows.h>

#if defined HQ_WIN_DESKTOP_PLATFORM

#ifndef _M_AMD64
#	define HQ_NO_PURE_ATOMIC_INTEGER_64
#endif

#endif

/*---------specialized HQAtomic-------*/
template <class T> T HQWin32InterlockedExchange(volatile T* target, T value);
template <class T> T HQWin32InterlockedExchangeAdd(volatile T* target, T value);
template <class T> T HQWin32InterlockedIncrement(volatile T* target);
template <class T> T HQWin32InterlockedDecrement(volatile T* target);
template <class T> T HQWin32InterlockedOr(volatile T* target, T value);
template <class T> T HQWin32InterlockedAnd(volatile T* target, T value);
template <class T> T HQWin32InterlockedXor(volatile T* target, T value);

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

	volatile T m_value;
};

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger()
{
	HQ_ASSERT_ALIGN(this, sizeof(T));

	//HQWin32InterlockedExchange(&m_value, T());//unintialize value
}

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger(const T val)
{
	HQ_ASSERT_ALIGN(this, sizeof(T));

	HQWin32InterlockedExchange(&m_value, val);
}

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger(const HQAtomicInteger<T>& a2)
{
	HQ_ASSERT_ALIGN(this, sizeof(T));

	HQWin32InterlockedExchange(&m_value, a2.Get());
}

template <class T>
inline const T HQAtomicInteger<T>::Get() const
{
	register T returnVal = 0;
	HQWin32InterlockedExchange(&returnVal, m_value);

	return returnVal;
}

template <class T>
inline const T HQAtomicInteger<T>::Assign(const T& val)
{
	return HQWin32InterlockedExchange(&m_value, val);
}

template <class T>
inline void HQAtomicInteger<T>::AddAssign(T op2) 
{
	HQWin32InterlockedExchangeAdd(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::AndAssign(T op2) 
{
	HQWin32InterlockedAnd(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::OrAssign(T op2) 
{
	HQWin32InterlockedOr(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::XorAssign(T op2) 
{
	HQWin32InterlockedXor(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::Increment()
{
	HQWin32InterlockedIncrement(&m_value);
}

template <class T>
inline void HQAtomicInteger<T>::Decrement()
{
	HQWin32InterlockedDecrement(&m_value);
}

/*-----------integer specializations--------------*/
template <> class HQAtomic<hqint32> : public HQAtomicInteger<hqint32> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const hqint32 val): HQAtomicInteger(val){}
	HQAtomic(const HQAtomic<hqint32>& a2): HQAtomicInteger(a2) {}
};

template <> class HQAtomic<hquint32> : public HQAtomicInteger<hquint32> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const hquint32 val): HQAtomicInteger(val){}
	HQAtomic(const HQAtomic<hqint32>& a2): HQAtomicInteger(a2) {}
};

#ifndef HQ_NO_PURE_ATOMIC_INTEGER_64

template <> class HQAtomic<hqint64> : public HQAtomicInteger<hqint64> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const hqint64 val): HQAtomicInteger(val){}
	HQAtomic(const HQAtomic<hqint64>& a2): HQAtomicInteger(a2) {}
};

template <> class HQAtomic<hquint64> : public HQAtomicInteger<hquint64> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const hquint64 val): HQAtomicInteger(val){}
	HQAtomic(const HQAtomic<hqint64>& a2): HQAtomicInteger(a2) {}
};

#endif

template <> class HQAtomic<hqshort16> : protected HQAtomicInteger<hqint32> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const hqshort16 val): HQAtomicInteger(hqint32(val)){}
	HQAtomic(const HQAtomic<hqshort16>& a2): HQAtomicInteger(a2) {}

	operator const hqshort16 () const {return (hqshort16)Get();}
	const hqshort16 operator = (const hqshort16 val) {return (hqshort16)Assign(hqint32(val));}//return old value
	const hqshort16 operator = (const HQAtomic<hqshort16> a2) {return (hqshort16)Assign(a2);}//return old value
	bool operator == (const hqshort16 val) const {return (hqshort16)Get() == val;}

	void operator += ( hqshort16 op2) {  AddAssign( hqint32(op2)); }
	void operator |= ( hqshort16 op2) {  OrAssign( hqint32(op2)); }
	void operator ^= ( hqshort16 op2) {  XorAssign( hqint32(op2)); }
	void operator &= ( hqshort16 op2) {  AndAssign( hqint32(op2)); }
	void operator ++ ( ) {  Increment(); }
	void operator -- ( ) {  Decrement(); }
	void operator ++ ( int op2 ) {  Increment(); }//post fix
	void operator -- ( int op2 ) {  Decrement(); }//post fix
};

template <> class HQAtomic<hqushort16> : protected HQAtomicInteger<hquint32> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const hqushort16 val): HQAtomicInteger(hquint32(val)){}
	HQAtomic(const HQAtomic<hqushort16>& a2): HQAtomicInteger(a2) {}

	operator const hqushort16 () const {return (hqushort16)Get();}
	const hqushort16 operator = (const hqushort16 val) {return (hqushort16)Assign(hquint32(val));}//return old value
	const hqushort16 operator = (const HQAtomic<hqushort16> a2) {return (hqushort16)Assign(a2);}//return old value
	bool operator == (const hqushort16 val) const {return (hqushort16)Get() == val;}

	void operator += ( hqushort16 op2) {  AddAssign( hquint32(op2)); }
	void operator |= ( hqushort16 op2) {  OrAssign( hquint32(op2)); }
	void operator ^= ( hqushort16 op2) {  XorAssign( hquint32(op2)); }
	void operator &= ( hqushort16 op2) {  AndAssign( hquint32(op2)); }
	void operator ++ ( ) {  Increment(); }
	void operator -- ( ) {  Decrement(); }
	void operator ++ ( int op2 ) {  Increment(); }//post fix
	void operator -- ( int op2 ) {  Decrement(); }//post fix
};

/*-----------boolean specialization--------*/
template <> class HQAtomic<hqbool> : protected HQAtomicInteger<hqint32> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const hqbool val): HQAtomicInteger(hqint32(val)){}
	HQAtomic(const HQAtomic<hqbool>& a2): HQAtomicInteger(hqint32(a2.Get())) {}

	operator const hqbool () const {return (hqbool)Get();}
	const hqbool operator = (const hqbool val) {return (hqbool)Assign(hqint32(val));}//return old value
	const hqbool operator = (const HQAtomic<hqbool> a2) {return (hqbool)Assign(a2);}//return old value
	bool operator == (const hqbool val) const {return (hqbool)Get() == val;}
};

template <> class HQAtomic<bool> : protected HQAtomicInteger<hqint32> {
public:
	HQAtomic(): HQAtomicInteger(){}//unintialize value
	HQAtomic(const bool val): HQAtomicInteger(hqint32(val)){}
	HQAtomic(const HQAtomic<bool>& a2): HQAtomicInteger(hquint32(a2.Get())) {}

	operator const bool () const {return GetBool();}
	const bool operator = (const bool val) {return AssignBool(val);}//return old value
	const bool operator = (const HQAtomic<bool> a2) {return AssignBool(a2.GetBool());}//return old value
	bool operator == (const bool val) const {return GetBool() == val;}
protected:
	const bool GetBool() const {return HQAtomicInteger::Get() != 0;}
	const bool AssignBool(bool val) {
		return HQAtomicInteger::Assign(hqint32(val)) != 0;
	}
};
/*------------wrapper functions------------------*/
#if defined HQ_WIN_DESKTOP_PLATFORM
#	include <intrin.h>

#	define InterlockedOr _InterlockedOr
#	define InterlockedXor _InterlockedXor
#	define InterlockedAnd _InterlockedAnd
#	define InterlockedOr64 _InterlockedOr64
#	define InterlockedXor64 _InterlockedXor64
#	define InterlockedAnd64 _InterlockedAnd64

#elif (defined HQ_WIN_STORE_PLATFORM || defined HQ_WIN_PHONE_PLATFORM) && !defined _M_ARM
#	include <intrin.h>

#	define InterlockedOr _InterlockedOr
#	define InterlockedXor _InterlockedXor
#	define InterlockedAnd _InterlockedAnd

#endif


template <>
inline hqint32 HQWin32InterlockedExchange(volatile hqint32* target, hqint32 value)
{
	return InterlockedExchange((LONG*)target, value);
}

template <>
inline hqint32 HQWin32InterlockedExchangeAdd(volatile hqint32* target, hqint32 value)
{
	return InterlockedExchangeAdd((LONG*)target, value);
}

template <>
inline hqint32 HQWin32InterlockedIncrement(volatile hqint32* target)
{
	return InterlockedIncrement((LONG*)target);
}

template <>
inline hqint32 HQWin32InterlockedOr(volatile hqint32* target, hqint32 value)
{
	return InterlockedOr((LONG*)target, value);
}

template <>
inline hqint32 HQWin32InterlockedXor(volatile hqint32* target, hqint32 value)
{
	return InterlockedXor((LONG*)target, (LONG)value);
}

template <>
inline hqint32 HQWin32InterlockedAnd(volatile hqint32* target, hqint32 value)
{
	return InterlockedAnd((LONG*)target, value);
}

template <>
inline hqint32 HQWin32InterlockedDecrement(volatile hqint32* target)
{
	return InterlockedDecrement((LONG*)target);
}

/*-----------------------*/
template <>
inline hquint32 HQWin32InterlockedExchange(volatile hquint32* target, hquint32 value)
{
	return InterlockedExchange((LONG*)target, (LONG)value);
}

template <>
inline hquint32 HQWin32InterlockedExchangeAdd(volatile hquint32* target, hquint32 value)
{
	return InterlockedExchangeAdd((LONG*)target, (LONG)value);
}

template <>
inline hquint32 HQWin32InterlockedIncrement(volatile hquint32* target)
{
	return InterlockedIncrement((LONG*)target);
}

template <>
inline hquint32 HQWin32InterlockedOr(volatile hquint32* target, hquint32 value)
{
	return InterlockedOr((LONG*)target, (LONG)value);
}

template <>
inline hquint32 HQWin32InterlockedXor(volatile hquint32* target, hquint32 value)
{
	return InterlockedXor((LONG*)target, (LONG)value);
}

template <>
inline hquint32 HQWin32InterlockedAnd(volatile hquint32* target, hquint32 value)
{
	return InterlockedAnd((LONG*)target, (LONG)value);
}

template <>
inline hquint32 HQWin32InterlockedDecrement(volatile hquint32* target)
{
	return InterlockedDecrement((LONG*)target);
}

/*-----------------------*/

#ifndef HQ_NO_PURE_ATOMIC_INTEGER_64

template <>
inline hqint64 HQWin32InterlockedExchange(volatile hqint64* target, hqint64 value)
{
	return InterlockedExchange64(target, value);
}

template <>
inline hqint64 HQWin32InterlockedExchangeAdd(volatile hqint64* target, hqint64 value)
{
	return InterlockedExchangeAdd64(target, value);
}

template <>
inline hqint64 HQWin32InterlockedIncrement(volatile hqint64* target)
{
	return InterlockedIncrement64(target);
}

template <>
inline hqint64 HQWin32InterlockedOr(volatile hqint64* target, hqint64 value)
{
	return InterlockedOr64(target, value);
}

template <>
inline hqint64 HQWin32InterlockedXor(volatile hqint64* target, hqint64 value)
{
	return InterlockedXor64((LONGLONG*)target, (LONGLONG)value);
}

template <>
inline hqint64 HQWin32InterlockedAnd(volatile hqint64* target, hqint64 value)
{
	return InterlockedAnd64(target, value);
}

template <>
inline hqint64 HQWin32InterlockedDecrement(volatile hqint64* target)
{
	return InterlockedDecrement64((LONGLONG*)target);
}

/*-----------------------*/
inline hquint64 HQWin32InterlockedExchange(volatile hquint64* target, hquint64 value)
{
	return InterlockedExchange64((LONGLONG*)target, (LONGLONG)value);
}

template <>
inline hquint64 HQWin32InterlockedExchangeAdd(volatile hquint64* target, hquint64 value)
{
	return InterlockedExchangeAdd64((LONGLONG*)target, (LONGLONG)value);
}

template <>
inline hquint64 HQWin32InterlockedIncrement(volatile hquint64* target)
{
	return InterlockedIncrement64((LONGLONG*)target);
}

template <>
inline hquint64 HQWin32InterlockedOr(volatile hquint64* target, hquint64 value)
{
	return InterlockedOr64((LONGLONG*)target, (LONGLONG)value);
}

template <>
inline hquint64 HQWin32InterlockedAnd(volatile hquint64* target, hquint64 value)
{
	return InterlockedAnd64((LONGLONG*)target, (LONGLONG)value);
}

template <>
inline hquint64 HQWin32InterlockedXor(volatile hquint64* target, hquint64 value)
{
	return InterlockedXor64((LONGLONG*)target, (LONGLONG)value);
}

template <>
inline hquint64 HQWin32InterlockedDecrement(volatile hquint64* target)
{
	return InterlockedDecrement64((LONGLONG*)target);
}

#endif//#ifndef HQ_NO_PURE_ATOMIC_INTEGER_64


#if defined HQ_WIN_DESKTOP_PLATFORM
#	include <intrin.h>

#	undef InterlockedOr 
#	undef InterlockedXor 
#	undef InterlockedAnd 
#	undef InterlockedOr64 
#	undef InterlockedXor64 
#	undef InterlockedAnd64 

#elif (defined HQ_WIN_STORE_PLATFORM || defined HQ_WIN_PHONE_PLATFORM) && !defined _M_ARM
#	include <intrin.h>

#	undef InterlockedOr 
#	undef InterlockedXor 
#	undef InterlockedAnd 

#endif

#endif
