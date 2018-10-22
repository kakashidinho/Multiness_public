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

#pragma once

#include <HQDataStream.h>
#include <vector>
#include <istream>
#include <streambuf>

#include <Unknwn.h>

namespace Nes {
	namespace WinRT {
		namespace Utils {
			static inline void SAFE_RELEASE(IUnknown* com) {
				if (com) {
					com->Release();
					com = nullptr;
				}
			}

			HQDataReaderStream* LoadResourceFile(const char* file);
			bool ReadResourceFile(const char* file, std::vector<unsigned char>& readDataOut);
			bool ReadStreamData(HQDataReaderStream* stream, std::vector<unsigned char>& readDataOut);

			/*-------- ComWrapper ---------*/
			template <class T>
			class ComWrapper {
			public:
				ComWrapper(std::nullptr_t null)
					:ComWrapper()
				{}

				ComWrapper() : m_com(nullptr)
				{}

				ComWrapper(const ComWrapper& src)
					:ComWrapper()
				{
					operator = (src);
				}

				~ComWrapper() {
					SafeRelease();
				}

				ULONG SafeRelease() {
					ULONG re = 0;
					if (m_com) {
						re = m_com->Release();
						m_com = nullptr;
					}

					return re;
				}

				T* Get() { return m_com; }

				operator T*() { return Get(); }
				T** operator &() { return &m_com; }
				T* operator-> () { return m_com; }

				ComWrapper& operator=(const ComWrapper& rhs) {
					if (m_com != rhs.m_com)
					{
						SafeRelease();
						m_com = rhs.m_com;

						if (m_com)
						{
							auto newRefCount = m_com->AddRef();

#if defined DEBUG || defined _DEBUG
							int a = 0;//for debugging
#endif
						}
					}
					return *this;
				}

				operator bool() const {
					return m_com != nullptr;
				}

				bool operator == (const ComWrapper& rhs) const {
					return m_com == rhs.m_com;
				}

				bool operator == (T* com) const {
					return m_com == com;
				}

				bool operator != (const ComWrapper& rhs) const {
					return m_com != rhs.m_com;
				}

				bool operator != (T* com) const {
					return m_com != com;
				}
			private:
				T* m_com;
			};

			/*---------StorageFileBuf -------------*/
			class StorageFileBuf : public std::streambuf
			{
			public:
				explicit StorageFileBuf(Windows::Storage::StorageFile ^ file, std::size_t buff_sz = 256);
				~StorageFileBuf();

				virtual std::streampos seekoff(
					std::streamoff off, std::ios_base::seekdir way,
					std::ios_base::openmode which) override;

				virtual std::streampos seekpos(std::streampos sp, std::ios_base::openmode which) override;

			private:
				// overrides base class underflow()
				virtual int_type underflow() override;

				// copy ctor and assignment not implemented;
				// copying not allowed
				StorageFileBuf(const StorageFileBuf &);
				StorageFileBuf &operator= (const StorageFileBuf &);

			private:
				void ResetStream(size_t position);

				Windows::Storage::Streams::IRandomAccessStream ^ m_file;
				Windows::Storage::Streams::DataReader ^ m_reader;
				Platform::Array<byte>^ m_buffer;

				size_t m_bufferFilePosition;
			};
		}
	}
}