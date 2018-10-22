/*
Copyright (C) 2010-2016  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_DEFAUL_MEM_MANAGER_H
#define HQ_DEFAUL_MEM_MANAGER_H

#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <list>
#include <new>

#include"HQMemAlignment.h"
#include"HQPrimitiveDataType.h"
#include"HQAssert.h"

class HQDefaultMemManager
{
public:
	HQDefaultMemManager() {}
	HQDefaultMemManager(size_t maxMemorySize) {}

	void * Malloc(size_t size) { return malloc(size); }
	void Free(void *ptr) {free(ptr);}
};


class HQMemManagerVirtualClass
{
public:
	virtual void * Malloc(size_t size) { return malloc(size); }
	virtual void Free(void *ptr) {free(ptr);}
};

/*----------pool memory manager -- only allocates fixed size block--------
block size must >= size of pointer type
--------------------------------------------------------------------------*/
class HQPoolMemoryManager: public HQMemManagerVirtualClass
{
public:
	HQPoolMemoryManager(size_t blockSize, size_t maxBlocksPerPool, size_t initPools = 1, bool fixedPool = false, bool is16ByteAlign = false);
	HQPoolMemoryManager(void *preAllocatedPool , size_t blockSize,  size_t maxBlocks);
	~HQPoolMemoryManager();

	size_t GetMaxBlocksPerPool() const {return m_maxBlocksPerPool;}
	size_t GetBlockSize() const {return m_blockSize;}

	void * Malloc() ;//return free block
	void * Malloc(size_t size) ;//return NULL if <size> > block size
	void Free(void *ptr);
protected:
	bool AddNewPool();//allocate new pool if needed
	void InitFreeList(void *startBlock, size_t numBlocks);
	bool IsPointerPointsToMemoryBlockInPools(void *ptr);
	bool IsPointerPointsToMemoryBlockInPool(void *ptr, void* pool);
	static bool IsBlockSizeLargerThanOrEqualToSizeofPointer(size_t blockSize) {return blockSize >= sizeof(void*);}

	std::list< void *> m_memoryPools;
	void * m_firstFreeBlock;
	size_t m_blockSize;
	size_t m_maxBlocksPerPool;
	bool m_fixedPool;//will the pool grow if it is out of free blocks
	bool m_isOwnerOfPool;
	bool m_is16ByteAlign;

private:
	HQPoolMemoryManager(const HQPoolMemoryManager& src) {}//not allow copy constructor
};
inline HQPoolMemoryManager::HQPoolMemoryManager(size_t blockSize, size_t maxBlocksPerPool , size_t initPools, bool fixedPool, bool is16ByteAlign)
		:m_isOwnerOfPool(true) , m_is16ByteAlign(is16ByteAlign) ,
		 m_blockSize(blockSize), m_maxBlocksPerPool(maxBlocksPerPool), m_fixedPool(fixedPool), m_firstFreeBlock(NULL)
{
	HQ_ASSERT(IsBlockSizeLargerThanOrEqualToSizeofPointer(blockSize));

	if (m_is16ByteAlign)
	{
		size_t re = blockSize % 16;
		if (re != 0)
			m_blockSize += 16 - re;//ensure that block size is multiple of 16
	}

	if (initPools > 0) //create the initial pools
	{
		if (! AddNewPool())//must be able to allocate the first pool
			throw std::bad_alloc();

		for (size_t i = 1; i < initPools; ++i)
		{
			hqubyte8 *prevPool = (hqubyte8*)m_memoryPools.back();

			AddNewPool();

			void *newPool = m_memoryPools.back();

			//point last block in prev pool to the fist block in the new pool
			*((void**)(prevPool + (m_maxBlocksPerPool - 1)* m_blockSize)) = newPool ;
		}

		m_firstFreeBlock = m_memoryPools.front();//first free block is the first block in first pool
	}
}

inline HQPoolMemoryManager::HQPoolMemoryManager(void *preAllocatedPool ,size_t blockSize,   size_t maxBlocks)
		:m_isOwnerOfPool(false)  , m_blockSize(blockSize), m_maxBlocksPerPool(maxBlocks) , m_fixedPool(true),
		 m_firstFreeBlock(preAllocatedPool)
{
	HQ_ASSERT(IsBlockSizeLargerThanOrEqualToSizeofPointer(blockSize));

	HQ_ASSERT(preAllocatedPool != NULL);

	this->InitFreeList(preAllocatedPool, m_maxBlocksPerPool);

	m_memoryPools.push_back(preAllocatedPool);
}

inline HQPoolMemoryManager::~HQPoolMemoryManager()
{
	if (m_isOwnerOfPool)
	{
		for (std::list<void*>::iterator ite = m_memoryPools.begin();
			 ite != m_memoryPools.end();
			 ++ite)
		{
			if (m_is16ByteAlign)
				HQAligned16Free(*ite);
			else
				free(*ite);
		}
	}
}

inline void HQPoolMemoryManager::InitFreeList(void *startBlock, size_t numBlocks)
{
	hq_ubyte8 * block = (hq_ubyte8*)startBlock;
	for (size_t i = numBlocks - 1 ; i > 0 ; --i)
	{
		*((void**)(block + (i - 1) * m_blockSize)) = (block + i * m_blockSize);//each block points to next block
	}

	*((void**)(block + (numBlocks - 1)* m_blockSize)) = NULL ;//last block points to NULL
}

inline void * HQPoolMemoryManager::Malloc()
{
	if (m_firstFreeBlock == NULL)
	{
		if (m_fixedPool)//cannot grow
			return NULL;

		if (!AddNewPool())//add new pool
			return NULL;
	}

	void * freeBlock = m_firstFreeBlock;

	m_firstFreeBlock = *(void**)m_firstFreeBlock;//set next free block as the first free block

	return freeBlock;
}

inline void * HQPoolMemoryManager::Malloc(size_t size)
{
	if (size > m_blockSize)
		return NULL;
	return this->Malloc();
}

inline void HQPoolMemoryManager::Free(void *ptr)
{
	if (ptr == NULL)
		return;
	HQ_ASSERT(this->IsPointerPointsToMemoryBlockInPools(ptr));
	//push to free list
	*(void**)ptr = m_firstFreeBlock;

	m_firstFreeBlock = ptr;
}

inline bool HQPoolMemoryManager::AddNewPool()
{
	void *newPool;
	if (m_is16ByteAlign)
	{
		newPool = HQAligned16Malloc(m_blockSize * m_maxBlocksPerPool);
	}
	else
		newPool = malloc(m_blockSize * m_maxBlocksPerPool);

	if (newPool == NULL)
		return false;
	m_firstFreeBlock = newPool;

	this->InitFreeList(newPool, m_maxBlocksPerPool);

	m_memoryPools.push_back(newPool);

	return true;
}

inline bool HQPoolMemoryManager::IsPointerPointsToMemoryBlockInPools(void *ptr)
{
	for (std::list<void*>::iterator ite = m_memoryPools.begin();
		 ite != m_memoryPools.end();
		 ++ite)
	{
		if (IsPointerPointsToMemoryBlockInPool(ptr, *ite))
			return true;
	}

	return false;
}

inline bool HQPoolMemoryManager::IsPointerPointsToMemoryBlockInPool(void *ptr, void* pool)
{
	if (ptr < pool)
		return false;
	size_t offset = (hq_ubyte8*)ptr - (hq_ubyte8*)pool ;
	size_t index = offset / m_blockSize;
	if (offset % m_blockSize != 0 || index >= m_maxBlocksPerPool)
		return false;
	return true;
}

#endif
