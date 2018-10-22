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

#include "WinRTUtils.hpp"

#include <HQEngine/winstore/HQWinStoreFileSystem.h>
#include <HQEngine/winstore/HQWinStoreUtil.h>

namespace Nes {
	namespace WinRT {
		namespace Utils {
			HQDataReaderStream* LoadResourceFile(const char* file) {
				return HQWinStoreFileSystem::OpenFileForRead(file);
			}

			bool ReadResourceFile(const char* file, std::vector<unsigned char>& readDataOut) {
				auto stream = LoadResourceFile(file);
				if (!stream)
					return false;

				return ReadStreamData(stream, readDataOut);
			}

			bool ReadStreamData(HQDataReaderStream* stream, std::vector<unsigned char>& readDataOut) {
				auto totalSize = stream->TotalSize();

				try {
					readDataOut.resize(totalSize);
				}
				catch (...) {
					return false;
				}

				if (stream->ReadBytes(readDataOut.data(), totalSize, 1) != 1)
					return false;

				return true;
			}

			/*---------StorageFileBuf -------------*/
			StorageFileBuf::StorageFileBuf(Windows::Storage::StorageFile ^ file, std::size_t buff_sz)  
				:m_bufferFilePosition(0)
			{
				m_file = HQWinStoreUtil::Wait(file->OpenReadAsync());
				m_reader = nullptr;

				m_buffer = ref new Platform::Array<byte>(max(buff_sz, 1));

				ResetStream(0);
			}

			StorageFileBuf::~StorageFileBuf() {
				if (m_reader)
				{
					m_reader->DetachStream();
					m_reader = nullptr;
				}
			}

			// overrides base class underflow()
			std::streambuf::int_type StorageFileBuf::underflow() {
				if (gptr() < egptr()) // buffer not exhausted
					return traits_type::to_int_type(*gptr());

				if (m_reader == nullptr)
					return traits_type::eof();

				auto base = reinterpret_cast<char*>(m_buffer->Data);

				//read data to buffer
				auto re = HQWinStoreUtil::Wait(m_reader->LoadAsync(m_buffer->Length));

				if (re == 0)
					return traits_type::eof();

				try {
					if (re < m_buffer->Length)
					{
						//not enough data to fill the buffer, so read each available byte one by one
						for (unsigned int i = 0; i < re; ++i)
						{
							m_buffer[i] = m_reader->ReadByte();
						}

					}//if (re < m_buffer->Length)
					else
						m_reader->ReadBytes(m_buffer);
				}
				catch (...) {
					return traits_type::eof();
				}

				if (eback() < egptr())//if this is not the first fill after ResetStream(), increase the file pointer
				{
					m_bufferFilePosition += egptr() - eback();
				}
				// Set new buffer pointers
				setg(base, base, base + re);

				return traits_type::to_int_type(*gptr());
			}

			std::streampos StorageFileBuf::seekoff(
				std::streamoff off, std::ios_base::seekdir way,
				std::ios_base::openmode which)
			{
				if (!m_file)
					return traits_type::eof();

				std::streampos ipos;
				std::streampos buffer_cur = m_bufferFilePosition + (gptr() - eback());

				switch (way)
				{
				case std::ios_base::beg:
					ipos = off;
					break;
				case std::ios_base::cur:
					ipos = buffer_cur + off;
					break;
				case std::ios_base::end:
					ipos = m_file->Size + off;
					break;
				}

				if (which & std::ios_base::in)
				{
					if (ipos == buffer_cur)
						return ipos;//no change

					ipos = seekpos(ipos, std::ios_base::in);
				}
				else
					ipos = buffer_cur;

				return ipos;
			}

			std::streampos StorageFileBuf::seekpos(std::streampos sp, std::ios_base::openmode which)
			{
				if (sp < 0 || (size_t)sp > m_file->Size)
					return traits_type::eof();

				std::streampos buffer_start = m_bufferFilePosition;
				std::streampos buffer_end = m_bufferFilePosition + (egptr() - eback());

				if (sp >= buffer_start && sp < buffer_end)
				{
					//the position is still within buffer region. reposition the get pointer only
					auto get_ptr = eback() + (sp - buffer_start);
					setg(eback(), get_ptr, egptr());

					return sp;
				}

				ResetStream((size_t)sp);

				if (m_reader == nullptr)//failed to position the stream
					return traits_type::eof();

				return sp;
			}

			void StorageFileBuf::ResetStream(size_t position) {
				if (m_file == nullptr)
					return;

				if (m_reader) {
					m_reader->DetachStream();
					m_reader = nullptr;
				}

				m_file->Seek(0);
				m_bufferFilePosition = position;

				try {
					m_reader = ref new Windows::Storage::Streams::DataReader(m_file->GetInputStreamAt(position));
				}
				catch (...) {
					m_reader = nullptr;
				}

				//set buffer state as empty
				char *end = reinterpret_cast<char*>(m_buffer->Data + m_buffer->Length);
				setg(end, end, end);
			}
		}
	}
}