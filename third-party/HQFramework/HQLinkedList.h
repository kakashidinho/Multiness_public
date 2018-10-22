/*
Copyright (C) 2010-2016  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_LINKED_LIST
#define HQ_LINKED_LIST
#include"HQMemoryManager.h"
#include"HQSharedPointer.h"

#include <new>

#ifdef new
#ifdef _MSC_VER
#pragma push_macro("new")
#define HQ_NEW_DEFINED
#undef new
#else
#	error new defined
#endif
#endif

/*---------------------------------------------
double linked list
---------------------------------------------*/
template <class T>
struct HQLinkedListNode
{
	HQLinkedListNode(HQLinkedListNode<T> * pNext , HQLinkedListNode<T> * pPrev , const T &element )
		: m_pNext (pNext) , m_pPrev(pPrev) , m_element(element)
	{
	}

	T m_element;
	HQLinkedListNode<T> * m_pNext;
	HQLinkedListNode<T> * m_pPrev;
};

/*----------------------------------------------------------------------------------------------------
class MemManager will be used for malloc and free memory block that has size sizeof(HQLinkedListNode<T>) 
-----------------------------------------------------------------------------------------------------*/
template <class T , class MemManager = HQDefaultMemManager>
class HQLinkedList
{
public:
	typedef HQLinkedListNode<T> LinkedListNodeType;
	typedef T LinkedListItemType;

	/*------iterator class - for traversing through list of items---*/
	class Iterator
	{
		friend class HQLinkedList;
	private:
		HQLinkedListNode<T> *const* m_ppRoot;
		HQLinkedListNode<T> * m_pCurrentNode;
	public:
		Iterator() : m_ppRoot (NULL) , m_pCurrentNode(NULL) {}
		Iterator& operator ++();//prefix addition  . shift to next node
		Iterator operator ++(int32_t);//suffic addition . shift to next node

		Iterator& operator --();//prefix subtraction . shift to prev node
		Iterator operator --(int32_t);//suffix subtraction . shift to prev node

		bool IsAtBegin(){return (m_pCurrentNode == *m_ppRoot);};//is at first node location
		bool IsAtEnd(){return (m_pCurrentNode == NULL);};//is at invalid location (ie location after the last node)

		void Rewind(){m_pCurrentNode = m_ppRoot;};//go to first node
		void ToLastItem() {if (*m_ppRoot != NULL) m_pCurrentNode = (*m_ppRoot)->m_pPrev; }//go to last item

		T* operator->();
		T& operator *() ;//unsafe
		T* GetPointer() {if (m_pCurrentNode) return &m_pCurrentNode->m_element; return NULL;}
		T* GetPointerNonCheck() {return &m_pCurrentNode->m_element;}
		HQLinkedListNode<T> * GetNode() {return  m_pCurrentNode;}

		void Invalid() {m_ppRoot = NULL; m_pCurrentNode = NULL;}
	};
	/*--------member functions------------*/
	HQLinkedList(const HQLinkedList &src ,const HQSharedPtr<MemManager> &pMemoryManager = HQ_NEW MemManager());
	HQLinkedList(const HQSharedPtr<MemManager> &pMemoryManager = HQ_NEW MemManager()) : m_pRoot(NULL) , m_size(0) , m_pMemManager(pMemoryManager) {}
	~HQLinkedList() {RemoveAll();}

	HQSharedPtr<MemManager> GetMemManager() const { return m_pMemManager; }

	HQLinkedList & operator = (const HQLinkedList &src);//copy operator
	HQLinkedList & TransferFrom (HQLinkedList &src);//transfer content of list <src> to this list.<src> will be empty

	HQLinkedList & operator << (const T & val);//add to the back

	inline HQLinkedListNode<T> * GetRoot() {return m_pRoot;}//get first node in the list
	inline const HQLinkedListNode<T> * GetRoot() const {return m_pRoot;}//get first node in the list
	inline uint32_t GetSize() const {return m_size;}
	inline T& GetFront() {return this->m_pRoot->m_element;}//get first element
	inline const T& GetFront() const {return this->m_pRoot->m_element;}//get first element
	inline T& GetBack() {return this->m_pRoot->m_pPrev->m_element;}//get last element
	inline const T& GetBack() const {return this->m_pRoot->m_pPrev->m_element;}//get last element

	HQLinkedListNode<T> * PushBack(const T & val);//add new element at the end of the list.Data will be copied from <val> using copy constructor
	HQLinkedListNode<T> * PushFront(const T &val);//add new element at the beginning of the list.Data will be copied from <val> using copy constructor
	HQLinkedListNode<T> * AddAfter(HQLinkedListNode<T> *node , const T& val);//add new element after node <node>.Data will be copied from <val> using copy constructor

	void PopBack();//remove element at the end of the list
	void PopFront();//remove element at the beginning of the list
	void RemoveAt(HQLinkedListNode<T> *node);
	void RemoveAll();

	void GetIterator(Iterator& iterator) const;//get iterator starting at the first item
	void GetIteratorFromLastItem(Iterator& iterator) const;//get iterator starting at the last item

	///use these methods with care. The element must be initialized after these methods are called because the constructor of the new element is not called by these methods
	HQLinkedListNode<T> * PushBack();//add a new element at the end. The new element is not initialized, returned pointer can be used to initialize the new element
	HQLinkedListNode<T> * PushFront();//add a new element at the beginning. The new element is not initialized, returned pointer can be used to initialize the new element
	HQLinkedListNode<T> * AddAfter(HQLinkedListNode<T> *node);//add new element after node <node>. The new element is not initialized, returned pointer can be used to initialize the new element
private:
	HQLinkedListNode<T> * AddFirst(const T &val);//add first element
	HQLinkedListNode<T> * AddFirst();//add first element. the element is not initialized
	HQLinkedListNode<T> * MallocNewNode();
	void FreeNode(HQLinkedListNode<T> * pNode);
	/*----attributes---------*/
	HQLinkedListNode<T> * m_pRoot;
	uint32_t m_size;

	HQSharedPtr<MemManager> m_pMemManager;
};
template <class T , class MemManager>
inline HQLinkedList<T , MemManager> :: HQLinkedList(const HQLinkedList<T , MemManager> &src , const HQSharedPtr<MemManager> &pMemoryManager)
 : m_pRoot(NULL) , m_size(0) , m_pMemManager(pMemoryManager)
{
	const HQLinkedListNode<T> *pNode = src.GetRoot();
	for (uint32_t i = 0 ; i < src.m_size ; ++i ,pNode = pNode->m_pNext)
		this->PushBack(pNode->m_element);

}

template <class T , class MemManager>
inline HQLinkedList<T , MemManager> & HQLinkedList<T , MemManager> :: operator =(const HQLinkedList<T,MemManager> &src)
{
	this->RemoveAll();
	const HQLinkedListNode<T> *pNode = src.GetRoot();
	for (uint32_t i = 0 ; i < src.m_size ; ++i ,pNode = pNode->m_pNext)
		this->PushBack(pNode->m_element);
	return *this;
}

template <class T , class MemManager>
inline HQLinkedList<T , MemManager> & HQLinkedList<T , MemManager> :: TransferFrom(HQLinkedList<T,MemManager> &src)
{
	this->RemoveAll();
	m_pMemManager = src.m_pMemManager;
	m_pRoot = src.GetRoot();
	m_size = src.m_size;

	src.m_pRoot = NULL;
	src.m_size = 0;

	return *this;
}

template <class T , class MemManager>
inline void HQLinkedList<T, MemManager>::GetIterator(typename HQLinkedList<T, MemManager>::Iterator & iterator)const
{
	iterator.m_ppRoot = &this->m_pRoot;
	iterator.m_pCurrentNode = this->m_pRoot;
}

template <class T , class MemManager>
inline void HQLinkedList<T, MemManager>::GetIteratorFromLastItem(typename HQLinkedList<T, MemManager>::Iterator & iterator)const
{
	iterator.m_ppRoot = &this->m_pRoot;
	if (this->m_pRoot == NULL)
		iterator.m_pCurrentNode = NULL;
	else
		iterator.m_pCurrentNode = this->m_pRoot->m_pPrev;
}


template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>:: MallocNewNode()
{
	return static_cast<HQLinkedListNode<T> *>(this->m_pMemManager->Malloc(sizeof(HQLinkedListNode<T>)));
}

template <class T , class MemManager>
inline void HQLinkedList<T , MemManager>:: FreeNode(HQLinkedListNode<T> * pNode)
{
	pNode->m_element.~T();
	this->m_pMemManager->Free(pNode);
}

template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::AddFirst(const T &val)
{
	this->m_pRoot = this->MallocNewNode();
	if (this->m_pRoot != NULL)
	{
		this->m_pRoot = new (this->m_pRoot) HQLinkedListNode<T>(this->m_pRoot , this->m_pRoot , val);
		this->m_size++;
	}

	return this->m_pRoot;
}

template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::AddAfter(HQLinkedListNode<T> *node , const T& val)
{
	HQLinkedListNode<T> * l_pNewNode = this->MallocNewNode();
	if (l_pNewNode == NULL)
		return NULL;
	l_pNewNode = new (l_pNewNode) HQLinkedListNode<T>(node->m_pNext , node , val);
	node->m_pNext->m_pPrev = l_pNewNode;
	node->m_pNext = l_pNewNode;

	this->m_size++;
	return l_pNewNode;
}

template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::PushBack(const T & val)//add new element at the end of the list
{
	if (this->m_size == 0)
	{
		return this->AddFirst(val);
	}
	else
	{
		return this->AddAfter(this->m_pRoot->m_pPrev,val);
	}
}

template <class T, class MemManager>
inline HQLinkedList<T, MemManager> & HQLinkedList<T, MemManager>::operator << (const T & val)
{
	PushBack(val);
	return *this;
}

template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::PushFront(const T &val)//add new element at the beginning of the list
{
	HQLinkedListNode<T> * l_pNewNode = this->PushBack(val);
	if (l_pNewNode != NULL)
		this->m_pRoot = l_pNewNode;
	return l_pNewNode;
}
template <class T , class MemManager>
inline void HQLinkedList<T , MemManager>::PopBack()//remove element at the end of the list
{
	this->RemoveAt(this->m_pRoot->m_pPrev);
}
template <class T , class MemManager>
inline void HQLinkedList<T , MemManager>::PopFront()//remove element at the beginning of the list
{
	this->RemoveAt(this->m_pRoot);
}
template <class T , class MemManager>
inline void HQLinkedList<T , MemManager>::RemoveAt(HQLinkedListNode<T> *node)
{
	if (m_size == 0 || node == NULL)
		return;

	this->m_size--;

	node->m_pPrev->m_pNext = node->m_pNext;//point next pointer of prev node to next node of this node
	node->m_pNext->m_pPrev = node->m_pPrev;//point prev pointer of next node to prev node of this node
	
	if (this->m_size == 0)
		this->m_pRoot = NULL;
	else if (node == m_pRoot)
		this->m_pRoot = this->m_pRoot->m_pNext;//shift root to next node

	this->FreeNode(node);

}

template <class T , class MemManager>
inline void HQLinkedList<T , MemManager>::RemoveAll()
{
	HQLinkedListNode<T> *pCur = this->m_pRoot;
	HQLinkedListNode<T> *pNext;
	for (uint32_t i = 0 ; i < this->m_size ; ++i)
	{
		pNext = pCur->m_pNext;
		this->FreeNode(pCur);
		pCur = pNext;
	}
	this->m_size = 0;
	this->m_pRoot = NULL;
}

template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::AddFirst()
{
	this->m_pRoot = this->MallocNewNode();
	if (this->m_pRoot != NULL)
	{
		//not call constructor
		this->m_pRoot->m_pNext = this->m_pRoot;
		this->m_pRoot->m_pPrev = this->m_pRoot;

		this->m_size++;
	}

	return this->m_pRoot;
}

template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::AddAfter(HQLinkedListNode<T> *node)
{
	HQLinkedListNode<T> * l_pNewNode = this->MallocNewNode();
	if (l_pNewNode == NULL)
		return NULL;
	//not call constructor
	l_pNewNode->m_pNext = node->m_pNext;
	l_pNewNode->m_pPrev = node;

	node->m_pNext->m_pPrev = l_pNewNode;
	node->m_pNext = l_pNewNode;

	this->m_size++;
	return l_pNewNode;
}

template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::PushBack()//add new element at the end of the list
{
	if (this->m_size == 0)
	{
		return this->AddFirst();
	}
	else
	{
		return this->AddAfter(this->m_pRoot->m_pPrev);
	}
}
template <class T , class MemManager>
inline HQLinkedListNode<T> * HQLinkedList<T , MemManager>::PushFront()//add new element at the beginning of the list
{
	HQLinkedListNode<T> * l_pNewNode = this->PushBack();
	if (l_pNewNode != NULL)
		this->m_pRoot = l_pNewNode;
	return l_pNewNode;
}

/*-----------iterator class------------*/
template <class T , class MemManager>
inline typename HQLinkedList<T , MemManager>::Iterator HQLinkedList<T , MemManager>::Iterator::operator ++(int32_t i)
{
	Iterator preAdd = *this;
	if (!this->IsAtEnd())
	{
		if (this->m_pCurrentNode == (*this->m_ppRoot)->m_pPrev)//last node in list
			this->m_pCurrentNode = NULL;
		else
			this->m_pCurrentNode = this->m_pCurrentNode->m_pNext;
	}
	return preAdd;
}

template <class T , class MemManager>
inline typename HQLinkedList<T , MemManager>::Iterator& HQLinkedList<T , MemManager>::Iterator::operator ++()
{
	if (!this->IsAtEnd())
	{
		if (this->m_pCurrentNode == (*this->m_ppRoot)->m_pPrev)//last node in list
			this->m_pCurrentNode = NULL;
		else
			this->m_pCurrentNode = this->m_pCurrentNode->m_pNext;
	}
	return *this;
}

template <class T , class MemManager>
inline typename HQLinkedList<T , MemManager>::Iterator HQLinkedList<T , MemManager>::Iterator::operator --(int32_t i)
{
	Iterator preSub = *this;
	if (this->IsAtEnd() && (*this->m_ppRoot) != NULL)
		this->m_pCurrentNode = (*this->m_ppRoot)->m_pPrev;
	else if (!this->IsAtBegin())
	{
		this->m_pCurrentNode = this->m_pCurrentNode->m_pPrev;
	}
	return preSub;
}

template <class T , class MemManager>
inline typename HQLinkedList<T , MemManager>::Iterator& HQLinkedList<T , MemManager>::Iterator::operator --()
{
	if (this->IsAtEnd() && (*this->m_ppRoot) != NULL)
		this->m_pCurrentNode = (*this->m_ppRoot)->m_pPrev;
	else if (!this->IsAtBegin())
	{
		this->m_pCurrentNode = this->m_pCurrentNode->m_pPrev;
	}
	return *this;
}

template <class T , class MemManager>
inline T * HQLinkedList<T , MemManager>::Iterator::operator->()
{
	HQ_ASSERT(!this->IsAtEnd());

	return &this->m_pCurrentNode->m_element;
}

template <class T , class MemManager>
inline T & HQLinkedList<T , MemManager>::Iterator::operator *()
{
	HQ_ASSERT(!this->IsAtEnd());

	return this->m_pCurrentNode->m_element;
}

#if defined _MSC_VER && defined HQ_NEW_DEFINED
#pragma pop_macro("new")
#undef HQ_NEW_DEFINED
#endif

#endif
