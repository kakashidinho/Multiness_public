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

/*---------specialized HQAtomic-------*/

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

	mutable volatile T m_value;
};

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger()
{
	HQ_ASSERT_ALIGN(this, sizeof(T));
}

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger(const T val)
: m_value(val)
{
	HQ_ASSERT_ALIGN(this, sizeof(T));
}

template <class T>
inline HQAtomicInteger<T>::HQAtomicInteger(const HQAtomicInteger<T>& a2)
: m_value(a2.m_value)
{
	HQ_ASSERT_ALIGN(this, sizeof(T));
}

template <class T>
inline const T HQAtomicInteger<T>::Get() const
{
	return __sync_fetch_and_or(&m_value, 0);
}

template <class T>
inline const T HQAtomicInteger<T>::Assign(const T& val)
{
	T prev;
    do {
        prev = m_value;
    } while (__sync_val_compare_and_swap(&m_value, prev, val) != prev);
    return prev;
}

template <class T>
inline void HQAtomicInteger<T>::AddAssign(T op2) 
{
	__sync_fetch_and_add(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::AndAssign(T op2) 
{
	__sync_fetch_and_and(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::OrAssign(T op2) 
{
	__sync_fetch_and_or(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::XorAssign(T op2) 
{
	__sync_fetch_and_xor(&m_value, op2);
}

template <class T>
inline void HQAtomicInteger<T>::Increment()
{
	__sync_fetch_and_add(&m_value, 1);
}

template <class T>
inline void HQAtomicInteger<T>::Decrement()
{
	__sync_fetch_and_sub(&m_value, 1);
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

#ifndef HQ_NO_PURE_ATOMIC_INTEGER_64

template <> class HQAtomic<hqint64> : public HQAtomicInteger<hqint64> {
	typedef HQAtomicInteger<hqint64 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const hqint64 val): ParentType(val){}
	HQAtomic(const HQAtomic<hqint64>& a2): ParentType(a2) {}
};

template <> class HQAtomic<hquint64> : public HQAtomicInteger<hquint64> {
	typedef HQAtomicInteger<hquint64 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const hquint64 val): ParentType(val){}
	HQAtomic(const HQAtomic<hqint64>& a2): ParentType(a2) {}
};

#endif

template <> class HQAtomic<hqshort16> : protected HQAtomicInteger<hqshort16> {
	typedef HQAtomicInteger<hqshort16 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const hqshort16 val): ParentType(val){}
	HQAtomic(const HQAtomic<hqshort16>& a2): ParentType(a2) {}
};

template <> class HQAtomic<hqushort16> : protected HQAtomicInteger<hqushort16> {
	typedef HQAtomicInteger<hqushort16 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const hqushort16 val): ParentType(val){}
	HQAtomic(const HQAtomic<hqushort16>& a2): ParentType(a2) {}
};

/*-----------boolean specialization--------*/
template <> class HQAtomic<hqbool> : protected HQAtomicInteger<hqint32> {
	typedef HQAtomicInteger<hqint32 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const hqbool val): ParentType(hqint32(val)){}
	HQAtomic(const HQAtomic<hqbool>& a2): ParentType(hqint32(a2.Get())) {}

	operator const hqbool () const {return (hqbool)Get();}
	const hqbool operator = (const hqbool val) {return (hqbool)Assign(hqint32(val));}//return old value
	const hqbool operator = (const HQAtomic<hqbool> a2) {return (hqbool)Assign(a2);}//return old value
	bool operator == (const hqbool val) const {return (hqbool)Get() == val;}
};

template <> class HQAtomic<bool> : protected HQAtomicInteger<hqint32> {
	typedef HQAtomicInteger<hqint32 > ParentType;
public:
	HQAtomic(): ParentType(){}//unintialize value
	HQAtomic(const bool val): ParentType(hqint32(val)){}
	HQAtomic(const HQAtomic<bool>& a2): ParentType(hquint32(a2.Get())) {}

	operator const bool () const {return GetBool();}
	const bool operator = (const bool val) {return AssignBool(val);}//return old value
	const bool operator = (const HQAtomic<bool> a2) {return AssignBool(a2.GetBool());}//return old value
	bool operator == (const bool val) const {return GetBool() == val;}
protected:
	const bool GetBool() const {return ParentType::Get() != 0;}
	const bool AssignBool(bool val) {
		return ParentType::Assign(hqint32(val)) != 0;
	}
};

#endif
