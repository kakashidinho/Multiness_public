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

#include "JniUtils.hpp"

#include <pthread.h>

namespace Nes {
	namespace Jni {

		void SetJVM(JavaVM *vm)
		{
			HQRemote::setJVM(vm);
		}

		JNIEnv * GetCurrenThreadJEnv()
		{
			return HQRemote::getCurrenThreadJEnv();
		}
		
		/*------ Buffer ---------*/
		Buffer::Buffer()
		: m_jBuffer(NULL), m_lengthBytes(0)
		{}
		
		Buffer::~Buffer() {
			auto jenv = GetCurrenThreadJEnv();
			if (m_jBuffer)
			{
				jenv->DeleteGlobalRef(m_jBuffer);
			}
		}
		
		void Buffer::Init(jobject jbuffer, jint length, jint elemSizeInBytes) {
			auto jenv = GetCurrenThreadJEnv();
			
			if (jbuffer)
			{
				m_jBuffer = jenv->NewGlobalRef(jbuffer);
				m_length = length;
				m_elemSizeInBytes = elemSizeInBytes;
				m_lengthBytes = length * m_elemSizeInBytes;
				jenv->DeleteLocalRef(jbuffer);
			}
		}

		void* Buffer::FastLock(JNIEnv* jenv) {
			auto jBuffer = (jarray)GetJniHandle();
			if (jBuffer)
				return jenv->GetPrimitiveArrayCritical(jBuffer, NULL);
			return NULL;
		}

		void Buffer::FastUnlock(JNIEnv* jenv, void* lockedData) {
			auto jBuffer = (jarray)GetJniHandle();
			if (jBuffer)
				jenv->ReleasePrimitiveArrayCritical(jBuffer, lockedData, 0);
		}
		
		/*-------- ShortBuffer ----------*/
		ShortBuffer::ShortBuffer(JNIEnv* jenv, jint sizeInShorts) {
			jshortArray jBuffer = jenv->NewShortArray(sizeInShorts);
			Init(jBuffer, sizeInShorts, sizeof(jshort));
		}	
			
		void* ShortBuffer::Lock(JNIEnv* jenv) {
			auto jBuffer = (jshortArray)GetJniHandle();
			if (jBuffer)
				return jenv->GetShortArrayElements(jBuffer, NULL);
			return NULL;
		}
		
		void ShortBuffer::Unlock(JNIEnv* jenv, void* lockedData) {
			auto jBuffer = (jshortArray)GetJniHandle();
			if (jBuffer)
				jenv->ReleaseShortArrayElements(jBuffer, (jshort*)lockedData, 0);
		}
		
		void ShortBuffer::CopyElemsTo(JNIEnv* jenv, void* dataOut, size_t startElem, size_t numElems)
		{
			auto jBuffer = (jshortArray)GetJniHandle();
			if (jBuffer)
				jenv->GetShortArrayRegion(jBuffer, startElem, numElems, (jshort*)dataOut);
		}
		
		void ShortBuffer::CopyTo(JNIEnv* jenv, void* dataOut, size_t offset, size_t sizeInBytes)
		{
			auto jBuffer = (jshortArray)GetJniHandle();
			if (jBuffer)
				jenv->GetShortArrayRegion(jBuffer, offset / GetElemSizeInBytes(), sizeInBytes / GetElemSizeInBytes(), (jshort*)dataOut);
		}

		void ShortBuffer::CopyFrom(JNIEnv* jenv, const void* dataIn, size_t offset, size_t sizeInBytes) {
			auto jBuffer = (jshortArray)GetJniHandle();
			if (jBuffer)
				jenv->SetShortArrayRegion(jBuffer, offset / GetElemSizeInBytes(), sizeInBytes / GetElemSizeInBytes(), (jshort*)dataIn);
		}
		
		/*-------- ByteBuffer ----------*/
		ByteBuffer::ByteBuffer(JNIEnv* jenv, jint sizeInBytes) {
			jbyteArray jBuffer = jenv->NewByteArray(sizeInBytes);
			Init(jBuffer, sizeInBytes, 1);
		}	
			
		void* ByteBuffer::Lock(JNIEnv* jenv) {
			auto jBuffer = (jbyteArray)GetJniHandle();
			if (jBuffer)
				return jenv->GetByteArrayElements(jBuffer, NULL);
			return NULL;
		}
		
		void ByteBuffer::Unlock(JNIEnv* jenv, void* lockedData) {
			auto jBuffer = (jbyteArray)GetJniHandle();
			if (jBuffer)
				jenv->ReleaseByteArrayElements(jBuffer, (jbyte*)lockedData, 0);
		}
		
		void ByteBuffer::CopyTo(JNIEnv* jenv, void* dataOut, size_t offset, size_t sizeInBytes)
		{
			auto jBuffer = (jbyteArray)GetJniHandle();
			if (jBuffer)
				jenv->GetByteArrayRegion(jBuffer, offset, sizeInBytes, (jbyte*)dataOut);
		}

		void ByteBuffer::CopyFrom(JNIEnv* jenv, const void* dataIn, size_t offset, size_t sizeInBytes) {
			auto jBuffer = (jbyteArray)GetJniHandle();
			if (jBuffer)
				jenv->SetByteArrayRegion(jBuffer, offset, sizeInBytes, (jbyte*)dataIn);
		}
		
		void ByteBuffer::CopyElemsTo(JNIEnv* jenv, void* dataOut, size_t startElem, size_t numElems)
		{
			auto jBuffer = (jbyteArray)GetJniHandle();
			if (jBuffer)
				jenv->GetByteArrayRegion(jBuffer, startElem, numElems, (jbyte*)dataOut);
		}
	}
}
