/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_DEAFULT_DATA_STREAM_H
#define HQ_DEAFULT_DATA_STREAM_H

#include "HQDataStream.h"

#include <stdio.h>
#include <string.h>

class HQSTDDataReader: public HQDataReaderStream
{
public:
	HQSTDDataReader(const char *file)
	{
		//copy name
		size_t nameLen = strlen(file);
		m_name = new char[nameLen + 1];
		strncpy(m_name, file, nameLen);
		m_name[nameLen] = '\0';

		//open file stream
		m_file = fopen(file, "rb");
		if (m_file != NULL)
		{
			fseek(m_file, 0, SEEK_END);
			m_totalSize = ftell(m_file);
			rewind(m_file);
		}
		else
		{
			m_totalSize = 0;
		}
	}
	~HQSTDDataReader()
	{
		Close();
		delete[] m_name;
	}

	hqint32 GetByte()
	{
		if (m_file == NULL)
			return EOF;
		return fgetc(m_file);
	}
	size_t ReadBytes(void* dataOut, size_t elemSize, size_t elemCount)
	{
		if (m_file == NULL)
			return 0;
		return fread(dataOut, elemSize, elemCount, m_file);
	}


	hqint32 Close()
	{
		if (m_file != NULL)
		{
			int re = fclose(m_file);
			m_file = NULL;

			return re;
		}

		return EOF;
	}
	hqint32 Seek(hqint64 offset, StreamSeekOrigin origin)
	{
		if (m_file == NULL)
			return EOF;

		return fseek(m_file, offset, origin);
	}

	void Rewind()
	{
		if (m_file != NULL)
			rewind(m_file);
	}

	hqint64 Tell() const//ftell implemetation
	{
		if (m_file == NULL)
			return 0;
		return ftell(m_file);
	}

	size_t TotalSize() const {return m_totalSize;}
	bool Good() const {return Tell() < TotalSize();}

	const char *GetName() const {return m_name;}
private:
	FILE * m_file;
	size_t m_totalSize;
	char * m_name;
};

#endif
