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

#ifndef NstApiMemoryStream_h
#define NstApiMemoryStream_h

#include <istream>
#include <streambuf>
#include <new>

namespace Nes {
	namespace Api {
		class MemInputStreamBuf: public std::streambuf
		{
		public:
			//data is moved into this object
			MemInputStreamBuf(char*&& data, size_t size)
			: m_data(data), m_size(size)
			{
				data = NULL;
				
				setg(m_data, m_data, m_data + m_size);
			}
			
			MemInputStreamBuf(const char* data, size_t size)
			: m_data((char*)malloc(size)), m_size(size)
			{
				if (m_data == NULL)
					throw std::bad_alloc();
				
				memcpy(m_data, data, size);
				
				setg(m_data, m_data, m_data + m_size);
			}
			~MemInputStreamBuf()
			{
				free(m_data);
			}
			
		private:
			virtual std::streampos seekoff (
											std::streamoff off, std::ios_base::seekdir way,
											std::ios_base::openmode which) override
			{
				std::streampos ipos;
				
				std::streampos cur = gptr() - m_data;
				
				switch(way)
				{
					case std::ios_base::beg:
						ipos = off;
						break;
					case std::ios_base::cur:
						ipos = cur + off;
						break;
					case std::ios_base::end:
						ipos = m_size + off;
						break;
				}
				
				if (which & std::ios_base::in)
					ipos = seekpos(ipos, std::ios_base::in);
				else
					ipos = cur;
				
				return ipos;
			}
			
			virtual std::streampos seekpos (std::streampos sp, std::ios_base::openmode which) override
			{
				if (sp < 0 || sp > m_size)
					return -1;
				
				setg(m_data, m_data + (size_t)sp, m_data + m_size);
				
				return sp;
			}
			
			char* m_data;
			size_t m_size;
		};
		
		class MemInputStream: public std::istream
		{
		public:
			MemInputStream(char*&& data, size_t size)
			: std::istream(NULL), m_sb(std::forward<char*>(data), size)
			{
				rdbuf(&m_sb);
			}
			
			MemInputStream(const char* data, size_t size)
			: std::istream(NULL), m_sb(data, size)
			{
				rdbuf(&m_sb);
			}
		private:
			MemInputStreamBuf m_sb;
		};
	}
}

#endif /* NstApiMemoryStream_h */
