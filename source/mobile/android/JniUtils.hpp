////////////////////////////////////////////////////////////////////////////////////////
//
// Multiness - NES/Famicom emulator written in C++
// Based on Nestopia emulator
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Multiness.
//
// Multiness is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Multiness is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Multiness; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef JniUtils_hpp
#define JniUtils_hpp

#include "../../../third-party/RemoteController/android/JniUtils.h"

#include <stdio.h>

#include <jni.h>

namespace Nes {
	namespace Jni {
		void SetJVM(JavaVM *vm);
		JNIEnv * GetCurrenThreadJEnv();
		
		typedef HQRemote::JMethodID MethodID;
		
		class Buffer {
		public:
			virtual ~Buffer();
		
			jint GetNumElems() const { return m_length; }
			jint GetElemSizeInBytes() const { return m_elemSizeInBytes; }
			jint GetSizeInBytes() const { return m_lengthBytes; }
			operator jobject() { return GetJniHandle(); }
			jobject GetJniHandle() { return m_jBuffer; }
			
			virtual void* Lock(JNIEnv* jenv) = 0;
			virtual void Unlock(JNIEnv* jenv, void* lockedData) = 0;

			void* FastLock(JNIEnv* jenv);
			void FastUnlock(JNIEnv* jenv, void* lockedData);

			virtual void CopyTo(JNIEnv* jenv, void* dataOut, size_t offset, size_t sizeInBytes) = 0;
			virtual void CopyFrom(JNIEnv* jenv, const void* dataIn, size_t offset, size_t sizeInBytes) = 0;
			virtual void CopyElemsTo(JNIEnv* jenv, void* dataOut, size_t startElem, size_t numElems) = 0;
		protected:
			Buffer();
			
			void Init(jobject jbuffer, jint length, jint elemSizeInBytes);//this must be called once only
			
		private:
			jobject m_jBuffer;
			jint m_length;
			jint m_lengthBytes;
			jint m_elemSizeInBytes;
		};
		
		class ShortBuffer: public Buffer{
		public:
			ShortBuffer(JNIEnv* jenv, jint sizeInShorts);
			
			virtual void* Lock(JNIEnv* jenv) override;
			virtual void Unlock(JNIEnv* jenv, void* lockedData) override;
			virtual void CopyTo(JNIEnv* jenv, void* dataOut, size_t offset, size_t sizeInBytes) override;
			virtual void CopyFrom(JNIEnv* jenv, const void* dataIn, size_t offset, size_t sizeInBytes) override;
			virtual void CopyElemsTo(JNIEnv* jenv, void* dataOut, size_t startElem, size_t numElems) override;
		};
		
		class ByteBuffer: public Buffer{
		public:
			ByteBuffer(JNIEnv* jenv, jint sizeInBytes);
			
			virtual void* Lock(JNIEnv* jenv) override;
			virtual void Unlock(JNIEnv* jenv, void* lockedData) override;
			virtual void CopyTo(JNIEnv* jenv, void* dataOut, size_t offset, size_t sizeInBytes) override;
			virtual void CopyFrom(JNIEnv* jenv, const void* dataIn, size_t offset, size_t sizeInBytes) override;
			virtual void CopyElemsTo(JNIEnv* jenv, void* dataOut, size_t startElem, size_t numElems) override;
		};
	}
}


#endif /* JniUtils_hpp */
