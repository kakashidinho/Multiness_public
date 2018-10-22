/*
Copyright (C) 2010-2014  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#ifndef HQ_ASSET_DATA_STREAM_ANDROID_H
#define HQ_ASSET_DATA_STREAM_ANDROID_H

#include "HQDataStream.h"

#include <string.h>
#include <android/asset_manager.h>

class HQAssetDataReaderAndroid: public HQDataReaderStream
{
public:
	HQAssetDataReaderAndroid(AAssetManager* manager, const char* file)
	{
		//copy name
		size_t nameLen = strlen(file);
		m_name = new char[nameLen + 1];
		strncpy(m_name, file, nameLen);
		m_name[nameLen] = '\0';
		
		//remove "assets/" part
		std::string finalFileName;
		std::string fileNameStd = file;
		size_t position = fileNameStd.find("assets/");
        if (0 == position) {
            //"assets/" is at the beginning of the path and we don't want it
            finalFileName = fileNameStd.substr(strlen("assets/"));
        } else {
            finalFileName = fileNameStd;
        }

		//open file stream
		m_file = AAssetManager_open(manager, finalFileName.c_str(), AASSET_MODE_RANDOM );
		if (m_file != NULL)
		{
			m_totalSize = AAsset_getLength(m_file);
		}
		else
		{
			m_totalSize = 0;
		}
	}
	~HQAssetDataReaderAndroid()
	{
		Close();
		delete[] m_name;
	}

	hqint32 GetByte()
	{
		if (m_file == NULL)
			return EOF;
		hqubyte8 c;
		size_t re = ReadBytes(&c, 1, 1);
		if (re == 0)
			return EOF;
		return c;
	}
	size_t ReadBytes(void* dataOut, size_t elemSize, size_t elemCount)
	{
		if (m_file == NULL)
			return 0;
			
		int bytes = AAsset_read(m_file, dataOut, elemSize * elemCount);
		if (bytes < 0)
			bytes = 0;
		size_t readElements = bytes / elemSize;
		return readElements;
	}


	hqint32 Close()
	{
		if (m_file != NULL)
		{
			AAsset_close(m_file);
			m_file = NULL;

			return 0;
		}

		return EOF;
	}
	hqint32 Seek(hqint64 offset, StreamSeekOrigin origin)
	{
		if (m_file == NULL)
			return EOF;
			
		off_t re = AAsset_seek(m_file, offset, origin);

		if (re == (off_t)-1)
			return -1;
		
		return 0;
	}

	void Rewind()
	{
		if (m_file != NULL)
			AAsset_seek(m_file, 0, SEEK_SET);
	}

	hqint64 Tell() const//ftell implemetation
	{
		if (m_file == NULL)
			return 0;
		return m_totalSize - AAsset_getRemainingLength(m_file);
	}

	size_t TotalSize() const {return m_totalSize;}
	bool Good() const {return Tell() < TotalSize();}

	const char *GetName() const {return m_name;}
private:
	AAsset * m_file;
	size_t m_totalSize;
	char * m_name;
};

#endif
