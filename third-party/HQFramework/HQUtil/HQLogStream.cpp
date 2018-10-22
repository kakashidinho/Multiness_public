/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "HQUtilPCH.h"
#include <stdio.h>
#include <new>
#include "../HQLogStream.h"

#if defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM

#include "../HQEngine/winstore/HQWinStoreFileSystem.h"
#include "../HQEngine/winstore/HQWinStoreUtil.h"

#include <ppltasks.h>

using namespace Windows::Storage;

#endif

class HQBaseLogStream : public HQLogStream
{
public:
	void Close() { delete this; }
};


class HQSTDLogStream : public HQBaseLogStream
{
public:
	void Flush() { if (file != NULL) fflush( file ); }
	void Log(const char *tag, const char * message) 
	{
		if (file != NULL)
			fprintf(file, "%s %s\n" , tag, message);
	}
	
protected:
	FILE *file;
};

class HQConsoleLogStream : public HQSTDLogStream
{
public:
	HQConsoleLogStream() { file = stdout; }
};

class HQFileLogStream : public HQSTDLogStream
{
public:
	HQFileLogStream(const char *filePath)
	{
		file = fopen(filePath, "w");
		if (file == NULL)
			throw std::bad_alloc();
	}

	~HQFileLogStream() {
		if (file != NULL) 
		{
			fclose(file); 
			file = NULL;
		}
	}
};

#if defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
class HQWinStoreFileLogStream : public HQBaseLogStream
{
public:
	HQWinStoreFileLogStream(const char *filePath)
	{
		file = HQWinStoreFileSystem::OpenOrCreateFileForWrite(filePath);
		if (file == nullptr)
			throw std::bad_alloc();

		file->UnicodeEncoding = Windows::Storage::Streams::UnicodeEncoding::Utf8;
	}

	~HQWinStoreFileLogStream() {
		if (file != nullptr) 
		{
			Flush();
			//file->Close();
			delete file;
			file = nullptr;
		}
	}

	void Flush() 
	{ 
		if (file != nullptr){
			HQWinStoreUtil::Wait(file->FlushAsync()); 
		}
	}
	void Log(const char *tag, const char * message) 
	{
		if (file != nullptr)
		{
			size_t tagLen = strlen(tag);
			size_t msgLen = strlen(message);

			//write tag
			for (size_t i = 0; i < tagLen; ++i)
			{
				file->WriteByte(tag[i]);
			}

			//write space
			file->WriteString(" ");

			//write message
			for (size_t i = 0; i < msgLen; ++i)
			{
				file->WriteByte(message[i]);
			}

			//write new line feed
			file->WriteString("\n");

			HQWinStoreUtil::Wait(file->StoreAsync()); 
		}
	}

private:
	Windows::Storage::Streams::DataWriter ^ file;
};
#endif//#if defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM


HQLogStream * HQCreateFileLogStream(const char *filePath)
{
	try{
#if defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM
		HQLogStream *stream = HQ_NEW HQWinStoreFileLogStream (filePath);
#else
		HQFileLogStream *stream = HQ_NEW HQFileLogStream(filePath);
#endif
		return stream;
	}
	catch (std::bad_alloc e)
	{
		return NULL;
	}
}
HQLogStream * HQCreateConsoleLogStream()
{
	return new HQConsoleLogStream();
}

#if defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM

class HQWin32DebugLogStream : public HQBaseLogStream
{
public:
	HQWin32DebugLogStream()
	{
	}

	~HQWin32DebugLogStream() {
	}

	void Flush() 
	{ 
	}
	void Log(const char *tag, const char * message) 
	{
#if defined _DEBUG || defined DEBUG
		//write HQEngine tag
		OutputDebugStringA("=========>HQEngine Debug Out:");

		//write user tag
		OutputDebugStringA(tag);

		//write space
		OutputDebugStringA(" ");

		//write message
		OutputDebugStringA(message);

		//write new line
		OutputDebugStringA("\n");
#endif
	}
};

HQLogStream * HQCreateDebugLogStream()
{
	return new HQWin32DebugLogStream();
}
#endif//#if defined WIN32 || defined HQ_WIN_PHONE_PLATFORM || defined HQ_WIN_STORE_PLATFORM

#ifdef HQ_ANDROID_PLATFORM

#include <android/log.h>

class HQLogCatStream : public HQBaseLogStream
{
public:

	void Flush() {  }
	void Log(const char *tag, const char * message) 
	{
		__android_log_print(ANDROID_LOG_DEBUG, tag, message);
	}
private:
};


HQLogStream * HQCreateLogCatStream()
{
	return new HQLogCatStream();
}
#endif
