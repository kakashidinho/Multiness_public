////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Nestopia.
//
// Nestopia is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Nestopia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Nestopia; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef NstRingBuffer_h
#define NstRingBuffer_h

#include <stdlib.h>
#include <stdint.h>

#include <algorithm>
#include <mutex>

#ifdef min
#	define NesCoreRingBufferMin min
#else
#	define NesCoreRingBufferMin std::min
#endif

namespace Nes
{
	namespace Core
	{
		template <class T>
		class RingBuffer {
		public:
			RingBuffer(size_t capacity);
			~RingBuffer();
			
			float FillRatio() const { return (float)Size() / Capacity(); }
			size_t Capacity() const { return m_capacity; }
			size_t Size() const;//readabe size
			bool Full() const { return m_full; }

			size_t Push(const T* dataIn, size_t numElems);
			size_t Pop(T* dataOut, size_t maxElems);
			template <class L>
			size_t Pop(T *dataOut, size_t maxElems, std::unique_lock<L> &lk);
			
			//<expectedSizeToWrite> = 0 to indicate the whole buffer will be written
			bool BeginWrite(T*& ptr1, size_t &size1, T*& ptr2, size_t &size2, size_t expectedSizeToWrite = 0);
			void EndWrite(T* ptr1, size_t size1, T* ptr2, size_t size2);
			
			bool BeginRead(const T*& ptr1, size_t &size1, const T*& ptr2, size_t &size2, size_t expectedSizeToRead = 0);
			void EndRead(const T* ptr1, size_t size1, const T* ptr2, size_t size2);
			
			void Clear();
		private:
			struct EmptyLock {
				void lock() {}
				void unlock() {}
			};

			T * m_data;
			
			size_t m_readIdx;
			size_t m_writeIdx;
			size_t m_capacity;
			bool m_full;
		};
		
		template <class T>
		inline RingBuffer<T>::RingBuffer(size_t capacity) {
			m_data = new T[capacity];
			m_capacity = capacity;
			
			m_readIdx = m_writeIdx = 0;
			m_full = false;
		}
		
		template <class T>
		inline RingBuffer<T>::~RingBuffer() {
			delete[] m_data;
		}
		
		template <class T>
		inline void RingBuffer<T>::Clear() {
			m_writeIdx = m_readIdx = 0;
			m_full = 0;
		}
		
		template <class T>
		inline size_t RingBuffer<T>::Size() const {
			size_t availableReadSize = 0;
			if (m_full)
				availableReadSize = m_capacity;
			else
				availableReadSize = (m_writeIdx - m_readIdx + m_capacity) % m_capacity;
			
			return availableReadSize;
		}
		
		template <class T>
		inline bool RingBuffer<T>::BeginWrite(T*& ptr1, size_t &size1, T*& ptr2, size_t &size2, size_t expectedSizeToWrite) {
			if (expectedSizeToWrite == 0)
				expectedSizeToWrite = m_capacity;
			size_t writeRegionSize[2];
			if (m_full)
			{
				writeRegionSize[0] = 0;
				writeRegionSize[1] = 0;
			}
			else if (m_readIdx <= m_writeIdx)
			{
				writeRegionSize[0] = m_capacity - m_writeIdx;
				writeRegionSize[1] = m_readIdx;
			}
			else {
				writeRegionSize[0] = m_readIdx - m_writeIdx;
				writeRegionSize[1] = 0;
			}
			
			writeRegionSize[0] = NesCoreRingBufferMin(writeRegionSize[0], expectedSizeToWrite);
			expectedSizeToWrite -= writeRegionSize[0];
			writeRegionSize[1] = NesCoreRingBufferMin(writeRegionSize[1], expectedSizeToWrite);
			
			ptr1 = writeRegionSize[0] > 0 ? (m_data + m_writeIdx): nullptr;
			size1 = writeRegionSize[0];
			
			ptr2 = writeRegionSize[1] > 0 ? m_data : nullptr;
			size2 = writeRegionSize[1];
			
			return writeRegionSize[0] > 0;
		}
		
		template <class T>
		inline void RingBuffer<T>::EndWrite(T* ptr1, size_t size1, T* ptr2, size_t size2) {
			if (!ptr1)
				size1 = 0;
			if (!ptr2)
				size2 = 0;
			auto totalSize = size1 + size2;
			m_writeIdx = (m_writeIdx + totalSize) % m_capacity;
			
			if (totalSize > 0 && m_writeIdx == m_readIdx)
			{
				//full
				m_full = true;
			}
		}
		
		template <class T>
		inline bool RingBuffer<T>::BeginRead(const T*& ptr1, size_t &size1, const T*& ptr2, size_t &size2, size_t expectedSizeToRead) {
			if (expectedSizeToRead == 0)
				expectedSizeToRead = m_capacity;

			size_t readRegionSize[2];
			if (m_full || m_writeIdx < m_readIdx)
			{
				readRegionSize[0] = m_capacity - m_readIdx;
				readRegionSize[1] = m_writeIdx;
			}
			else if (m_writeIdx > m_readIdx)
			{
				readRegionSize[0] = m_writeIdx - m_readIdx;
				readRegionSize[1] = 0;
			}
			else {
				readRegionSize[0] = 0;
				readRegionSize[1] = 0;
			}

			readRegionSize[0] = NesCoreRingBufferMin(readRegionSize[0], expectedSizeToRead);
			expectedSizeToRead -= readRegionSize[0];
			readRegionSize[1] = NesCoreRingBufferMin(readRegionSize[1], expectedSizeToRead);
			
			ptr1 = m_data + m_readIdx;
			size1 = readRegionSize[0];
			
			ptr2 = readRegionSize[1] > 0 ? m_data : nullptr;
			size2 = readRegionSize[1];
			
			return readRegionSize[0] > 0;
		}
		
		template <class T>
		inline void RingBuffer<T>::EndRead(const T* ptr1, size_t size1, const T* ptr2, size_t size2)
		{
			if (!ptr1)
				size1 = 0;
			if (!ptr2)
				size2 = 0;
			auto totalSize = size1 + size2;
			m_readIdx = (m_readIdx + totalSize) % m_capacity;
			
			if (totalSize > 0)
			{
				m_full = false;
				
				if (m_writeIdx == m_readIdx)
				{
					//empty
					m_writeIdx = 0;
					m_readIdx = 0;
				}
			}
		}
		
		template <class T>
		inline size_t RingBuffer<T>::Push(const T* srcData, size_t elems) {
			T* ptr[2];
			size_t writeRegionSize[2];
			
			BeginWrite(ptr[0], writeRegionSize[0], ptr[1], writeRegionSize[1]);
			
			auto remainElems = elems;
			size_t readOffset = 0;
			
			if (writeRegionSize[0])
			{
				size_t sizeToCopy = NesCoreRingBufferMin(remainElems, writeRegionSize[0]);
				
				memcpy(ptr[0], srcData + readOffset, sizeToCopy * sizeof(T));
				
				writeRegionSize[0] = sizeToCopy;
				
				remainElems -= sizeToCopy;
				readOffset += sizeToCopy;
				
				if (remainElems > 0 && writeRegionSize[1] > 0)
				{
					sizeToCopy = NesCoreRingBufferMin(remainElems, writeRegionSize[1]);
					
					memcpy(ptr[1], srcData + readOffset, sizeToCopy * sizeof(T));
					
					writeRegionSize[1] = sizeToCopy;
					
					remainElems -= sizeToCopy;
				}
				else
					writeRegionSize[1] = 0;
			}//if (writeRegionSize[0])
			
			EndWrite(ptr[0], writeRegionSize[0], ptr[1], writeRegionSize[1]);
			
			return elems - remainElems;
		}

		template <class T>
		inline size_t RingBuffer<T>::Pop(T *dataOut, size_t maxElems) {
			EmptyLock no_lock;
			std::unique_lock<EmptyLock> lk(no_lock);

			return Pop(dataOut, maxElems, lk);
		}
		
		template <class T> template <class L>
		inline size_t RingBuffer<T>::Pop(T *dataOut, size_t maxElems, std::unique_lock<L> &lk) {
			if (maxElems == 0)
				return 0;
			
			const T* ptr[2];
			size_t readRegionSize[2];
			
			BeginRead(ptr[0], readRegionSize[0], ptr[1], readRegionSize[1]);
			lk.unlock();//don't need to lock buffer when reading data from it
			
			//read each region in turn
			auto remainSizeToRead = maxElems;
			auto dataOut_ptr = dataOut;
			
			if (readRegionSize[0])
			{
				//read first region
				auto sizeToRead = NesCoreRingBufferMin(remainSizeToRead, readRegionSize[0]);
				auto sizeToReadBytes = sizeToRead * sizeof(T);
				
				memcpy(dataOut_ptr, ptr[0], sizeToReadBytes);
				
				readRegionSize[0] = sizeToRead;
				
				remainSizeToRead -= sizeToRead;
				dataOut_ptr += sizeToRead;
				
				if (remainSizeToRead > 0 && readRegionSize[1] > 0)
				{
					//read second region
					sizeToRead = NesCoreRingBufferMin(remainSizeToRead, readRegionSize[1]);
					sizeToReadBytes = sizeToRead * sizeof(T);
					
					memcpy(dataOut_ptr, ptr[1], sizeToReadBytes);
					
					readRegionSize[1] = sizeToRead;
					
					remainSizeToRead -= sizeToRead;
				}
				else
					readRegionSize[1] = 0;
			}//if (readRegionSize[0])
			
			lk.lock();//lock buffer to update its pointers
			EndRead(ptr[0], readRegionSize[0], ptr[1], readRegionSize[1]);
			
			return maxElems - remainSizeToRead;
		}
	}
}

#endif /* NstRingBuffer_h */
