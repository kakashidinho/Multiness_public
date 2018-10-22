/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "../HQEnginePCH.h"
#include "../HQEngineCommonInternal.h"
#include "../../HQBufferedDataStream.h"
#include "HQWinStoreFileSystem.h"
#include "HQWinStoreUtil.h"

using namespace HQEngineHelper;

#include <vector>
#include <ppltasks.h>
#include <string>
#include <thread>

#define CLAMP_TO_ZERO(a) ((a < 0)? 0: a)

namespace HQWinStoreFileSystem
{
#if USE_WIN32_HANDLE
	class  DataReader : public HQDataReaderStream
	{
	public:
		DataReader(const char *fileName, bool useExactFileName = false);
		~DataReader();

		int GetByte();
		size_t ReadBytes(void* dataOut, size_t elemSize, size_t elemCount);
		int Close();
		int Seek(long long offset, StreamSeekOrigin origin);
		void Rewind();
		long long Tell() const;//ftell implemetation

		size_t TotalSize() const { return totalSize; }
		bool Good() const { return Tell() < TotalSize(); }

		const char *GetName() const { return name; }
	private:
		size_t ReadBytes(unsigned char *ptr, size_t bytes);

		HANDLE fileHandle;

		LARGE_INTEGER currentPointer;
		size_t totalSize;//total size of stream from the start to the end

		char * name;//file name
	};
#else
	//async version
	class BufferedDataReader : public HQDataReaderStream
	{
	public:
		BufferedDataReader(Windows::Storage::Streams::IRandomAccessStream ^stream, const char *_name);
		~BufferedDataReader();

		int GetByte();
		size_t ReadBytes(void* dataOut, size_t elemSize, size_t elemCount);
		int Close();
		int Seek(long long offset, StreamSeekOrigin origin);
		void Rewind();
		long long Tell() const;//ftell implemetation

		size_t TotalSize() const { return totalSize; }
		bool Good() const { return Tell() < TotalSize(); }

		const char *GetName() const { return name; }
	private:
		void ResetStream(size_t position);
		void ReadBytes(unsigned char *ptr, size_t bytes);
		bool FillBuffer();

		Windows::Storage::Streams::IRandomAccessStream ^stream;
		Windows::Storage::Streams::DataReader ^reader;

		Platform::Array<unsigned char>^ buffer;
		size_t bufferSize;
		size_t unconsumedBufferLength;
		size_t bufferOffset;
		size_t streamOffset;
		size_t totalSize;//total size of stream from the start to the end

		char * name;//file name
	};
#endif

	//emu current working directory
	static Platform::String ^ g_currentDir = "";

	static HANDLE OpenFileHandleForRead(Windows::Storage::StorageFolder^ folder, Platform::String^ refFileName)
	{
		CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {0};
		extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
		extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		extendedParams.dwFileFlags = FILE_FLAG_RANDOM_ACCESS;
		extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
		extendedParams.lpSecurityAttributes = nullptr;
		extendedParams.hTemplateFile = nullptr;

		auto fullPath = folder != nullptr ? (folder->Path + L"\\" + refFileName) : refFileName;

		auto file = 
			CreateFile2(
				fullPath->Data(),
				GENERIC_READ,
				FILE_SHARE_READ,
				OPEN_EXISTING,
				&extendedParams
				);

		if (file == INVALID_HANDLE_VALUE)
			return nullptr;

		return file;
	}

	static Windows::Storage::StorageFile ^ OpenFile(Windows::Storage::StorageFolder^ folder, Platform::String^ refFileName)
	{
		using namespace Windows::Storage;
		using namespace Windows::Foundation;

		try
		{
			if (folder != nullptr)
				return HQWinStoreUtil::Wait(folder->GetFileAsync(refFileName));
			else
				return HQWinStoreUtil::Wait(Windows::Storage::StorageFile::GetFileFromPathAsync(refFileName));
		}
		catch (...)
		{
			return nullptr;
		}

	}

	static HANDLE OpenFileHandleForRead(Platform::String^ refFileName)
	{
		if (refFileName == nullptr)
			return nullptr;
		//try all possible paths
		Windows::Storage::StorageFolder^ folder = nullptr;
		auto file = OpenFileHandleForRead(nullptr, refFileName);

		if (file == nullptr)
		{
			folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
			file = OpenFileHandleForRead(folder, refFileName);
		}

		if (file == nullptr)
		{
			folder = Windows::Storage::ApplicationData::Current->LocalFolder;
			file = OpenFileHandleForRead(folder, refFileName);
		}
#if defined HQ_WIN_STORE_PLATFORM
		if (file == nullptr)
		{
			folder = Windows::Storage::ApplicationData::Current->RoamingFolder;
			file = OpenFileHandleForRead(folder, refFileName);
		}
		if (file == nullptr)
		{
			folder = Windows::Storage::ApplicationData::Current->TemporaryFolder;
			file = OpenFileHandleForRead(folder, refFileName);
		}
#endif

		return file;
	}

	static Windows::Storage::StorageFile ^ OpenFile(Platform::String^ refFileName)
	{
		if (refFileName == nullptr)
			return nullptr;
		//try all possible paths
		Windows::Storage::StorageFolder^ folder = nullptr;
		auto file = OpenFile(nullptr, refFileName);

		if (file == nullptr)
		{
			folder = Windows::ApplicationModel::Package::Current->InstalledLocation;
			file = OpenFile(folder, refFileName);
		}

		if (file == nullptr)
		{
			folder = Windows::Storage::ApplicationData::Current->LocalFolder;
			file = OpenFile(folder, refFileName);
		}
#if defined HQ_WIN_STORE_PLATFORM
		if (file == nullptr)
		{
			folder = Windows::Storage::ApplicationData::Current->RoamingFolder;
			file = OpenFile(folder, refFileName);
		}
		if (file == nullptr)
		{
			folder = Windows::Storage::ApplicationData::Current->TemporaryFolder;
			file = OpenFile(folder, refFileName);
		}
#endif

		return file;
	}

	static Windows::Storage::StorageFolder ^ GetOrCreateContainingDirectory
		(Windows::Storage::StorageFolder ^root, Platform::String^ refFileName)
	{
		using namespace Concurrency;

		const wchar_t *fileName = refFileName->Begin();

		Windows::Storage::StorageFolder ^ currentDir = root;

		size_t lastSlash = 0;
		size_t dirNameLen = 0;
		Windows::Storage::StorageFolder ^ dir = nullptr;
		//default task
		task<Windows::Storage::StorageFolder ^> createFolderTask;

		bool createDir = false;

		for (size_t i = 0; i < refFileName->Length(); ++i)
		{
			if (fileName[i] == L'\\')
			{
				dirNameLen = i - lastSlash;

				Platform::String^ subDirName = ref new Platform::String(fileName + lastSlash, dirNameLen);

				if (!createDir)
				{
					try{
						dir = HQWinStoreUtil::Wait( currentDir->GetFolderAsync(subDirName) );
					}
					catch (...)
					{
						dir = nullptr;
					}

					if (dir == nullptr)//need to create this folder and its subfolders
					{
						createDir = true;
						//create task chain
						createFolderTask = create_task(
							[currentDir] () -> Windows::Storage::StorageFolder ^
							{
								return currentDir;
							}
						);
					}
					else//just change the current folder to this folder
					{
						currentDir = dir;
					}
				}//if (!createDir)

				if (createDir)//create new directory
				{
					createFolderTask = createFolderTask.then(
						[subDirName] (Windows::Storage::StorageFolder ^ lastCreatedFolder)
						{
							return lastCreatedFolder->CreateFolderAsync(subDirName);
						}
						);
				}

				lastSlash = i + 1;
			}//if (fileName[i] == L'\\')
		}//for (size_t i = 0; i < refFileName->Length(); ++i)

		try{
			if (createDir)
				return HQWinStoreUtil::Wait(createFolderTask);
			else
				return currentDir;
		}
		catch (...)
		{
			return nullptr;
		}
	}

	//create file path string by concatenating with current directory, remove ".." and replacing '/' with '\\'
	static Platform::String^ CreateFixedPathString(const wchar_t *wString)
	{
		if (wString == NULL)
			return nullptr;
		
		size_t intendedPathPos = 0;
		size_t fixedPathPos = 0;
		size_t dotCount = 0;
		size_t len ;
		wchar_t* fixedPath;
		bool needSlash = false;
		std::vector<size_t> slashPositions; //positions of '\\' or '/' characters

		len = wcslen(wString);
		if (g_currentDir != L"")
		{
			if (g_currentDir->Begin()[g_currentDir->Length()-1] != L'\\')
			{
				len += g_currentDir->Length() + 1;
				needSlash = true;
			}
			else
				len += g_currentDir->Length();
		}

		fixedPath = HQ_NEW wchar_t[len + 1];
		//concatenate the current working directory at the beginning
		for (size_t i = 0; i < g_currentDir->Length(); ++i)
		{
			fixedPath[i] = g_currentDir->Begin()[i];
			if (g_currentDir->Begin()[i] == L'\\')
				slashPositions.push_back(i);
		}
		fixedPathPos = g_currentDir->Length();
		if (needSlash)
		{
			fixedPath[fixedPathPos] = L'\\';
			slashPositions.push_back(fixedPathPos);
			fixedPathPos += 1;
		}

		/*-----------concatenate the intended path----------*/

		while (wString[intendedPathPos] != L'\0')
		{
			if (wString[intendedPathPos] == L'/' || wString[intendedPathPos] == L'\\')
			{
				if (dotCount == 2)//this is "\..\" pop the last folder path
				{
					if (slashPositions.size() >= 2)
					{
						//pop the last folder
						slashPositions.pop_back();
						fixedPathPos = slashPositions.back();
					}
					else if (slashPositions.size() > 0)
					{
						//pop the last folder
						slashPositions.pop_back();
						fixedPathPos = (size_t)-1;
					}
				}
				else
				{
					fixedPath[fixedPathPos] = '\\';
					slashPositions.push_back(fixedPathPos);//push the slash position to positions list
				}

				dotCount = 0;//reset dot count
			}
			else
			{
				fixedPath[fixedPathPos] = wString[intendedPathPos]; 
				if (wString[intendedPathPos] == L'.')
				{
					dotCount++;
				}
				else
				{
					dotCount = 0;//reset dot count
				}
			}

			++intendedPathPos;
			if (fixedPathPos == (size_t)-1)
				fixedPathPos = 0;
			else
				++fixedPathPos;
		}

		fixedPath[fixedPathPos] = L'\0';

		Platform::String^ refString = ref new Platform::String( fixedPath);

		delete[] fixedPath;

		return refString;
	}

	
	//create file path string by concatenating with current directory, remove ".." and replacing '/' with '\\'
	static Platform::String^ CreateFixedPathString(const char *cstring)
	{
		if (cstring == NULL)
			return nullptr;

		// newsize describes the length of the 
		// wchar_t string called wcstring in terms of the number 
		// of wide characters, not the number of bytes.
		size_t newsize = strlen(cstring) + 1;

		// The following creates a buffer large enough to contain 
		// the exact number of characters in the original string
		// in the new format. If you want to add more characters
		// to the end of the string, increase the value of newsize
		// to increase the size of the buffer.
		wchar_t * wString = HQ_NEW wchar_t[newsize];

		// Convert char* string to a wchar_t* string.
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, wString, newsize, cstring, _TRUNCATE);

		Platform::String^ refString = CreateFixedPathString(wString);

		delete[] wString;

		return refString;
	}

	ReadDataResult::ReadDataResult(unsigned char*_pData, unsigned int _dataSize)
		:pData(_pData), dataSize(_dataSize)
	{
	}

	ReadDataResult::~ReadDataResult()
	{
		if (pData)
			delete[] pData;
	}

	bool ReadData(const char *fileName, unsigned char *&pDataOut, unsigned int &dataSizeOut)
	{
		if (fileName == NULL)
			return false;
		unsigned char *pData = NULL;
		unsigned int dataSize = 0;

		bool done = false;

		auto reader = OpenFileForRead(fileName);
		if (reader == nullptr)
			return false;
		try{
			dataSize = reader->TotalSize();
			pData = HQ_NEW unsigned char[dataSize];

			fread(pData, dataSize, 1, reader);
		}
		catch (...)
		{
			fclose(reader);
			return false;
		}

		fclose(reader);

		pDataOut = pData;
		dataSizeOut = dataSize;

		return true;
	}

	HQSharedPtr<ReadDataResult> ReadData(const char *fileName)
	{
		unsigned char *pData = NULL;
		unsigned int dataSize = 0;

		if (!ReadData(fileName, pData, dataSize))
			return NULL;

		return new ReadDataResult(pData, dataSize);
	}

	Windows::Storage::StorageFile ^ OpenFile(const char *fileName)
	{
		Platform::String^ refFileName = CreateFixedPathString( fileName);
	
		return OpenFile(refFileName);
	}

	Windows::Storage::StorageFile ^ OpenExactFile(const char *fileName)
	{
		Platform::String^ refFileName;
		int wStringSize = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, fileName, -1, NULL, 0);
		wchar_t *wFileName = new wchar_t[wStringSize];
		MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, fileName, -1, wFileName, wStringSize);
		refFileName = ref new Platform::String(wFileName);
		delete[] wFileName;

		return OpenFile(refFileName);
	}

	//open or create file in local folder
	Windows::Storage::StorageFile ^ OpenOrCreateFile(const char *fileName)
	{
		if (fileName == NULL)
			return nullptr;
		using namespace Windows::Storage;
		using namespace Windows::Foundation;

		Platform::String^ refFileName = CreateFixedPathString( fileName);
	
		auto folder = Windows::Storage::ApplicationData::Current->LocalFolder;

		auto file = OpenFile(folder, refFileName);
		if (file == nullptr)
		{
			try{
				//create new file
				if (GetOrCreateContainingDirectory(folder, refFileName) != nullptr)
				{
					file = HQWinStoreUtil::Wait(folder->CreateFileAsync(refFileName));
				}
			}
			catch (...)
			{
				file = nullptr;
			}
		}

		return file;
	}

	//open or create file in local folder
	Windows::Storage::Streams::DataWriter ^ OpenOrCreateFileForWrite(const char *fileName)
	{
		if (fileName == NULL)
			return nullptr;
		using namespace Windows::Storage;
		
		Platform::String^ refFileName = CreateFixedPathString( fileName);
		auto folder = Windows::Storage::ApplicationData::Current->LocalFolder;

		auto file = OpenFile(folder, refFileName);

		if (file != nullptr)
		{
			//delete file, because we will overwrite it
			HQWinStoreUtil::Wait(file->DeleteAsync());
		}

		file = OpenOrCreateFile(fileName);

		if (file == nullptr)
			return nullptr;

		auto stream = HQWinStoreUtil::Wait(  file->OpenAsync(FileAccessMode::ReadWrite));

		auto outputStream = stream->GetOutputStreamAt(0);

		return ref new Streams::DataWriter(outputStream);
	}

	//this will search file in installed folder, local folder, roaming folder, temp folder of the app
	HQDataReaderStream * OpenFileForRead(const char *filename)
	{
		if (filename == NULL)
			return nullptr;
#if USE_WIN32_HANDLE
		HQDataReaderStream * datareader = nullptr;

		try
		{
			DataReader *stream = new DataReader(filename);
			datareader = new HQBufferedDataReader(stream);
			stream->Release();
		}
		catch (...)
		{
			return nullptr;
		}
#else
		using namespace Windows::Storage;
		using namespace Concurrency;
		using namespace Windows::Storage::Streams;

		auto file = OpenFile(filename);
		if (file == nullptr)
			return nullptr;

		
		
		auto datareader = HQWinStoreUtil:: Wait(create_task(file->OpenAsync(FileAccessMode::Read))
			.then([filename] (Streams::IRandomAccessStream ^ stream) -> BufferedDataReader*
		{
			if (stream == nullptr)
				return nullptr;
			return new BufferedDataReader(stream, filename);
		}));
#endif

		return datareader;
	}
	
	//open a file using exact name
	HQDataReaderStream * OpenExactFileForRead(const char *filename)
	{
		if (filename == NULL)
			return nullptr;
#if USE_WIN32_HANDLE
		HQBufferedDataReader * datareader = nullptr;

		try
		{
			DataReader *stream = new DataReader(filename, true);
			datareader = new HQBufferedDataReader(stream);
			stream->Release();
		}
		catch (...)
		{
			return nullptr;
		}
#else
		using namespace Windows::Storage;
		using namespace Concurrency;
		using namespace Windows::Storage::Streams;

		auto file = OpenExactFile(filename);
		if (file == nullptr)
			return nullptr;

		
		
		auto datareader = HQWinStoreUtil:: Wait(create_task(file->OpenAsync(FileAccessMode::Read))
			.then([filename] (Streams::IRandomAccessStream ^ stream) -> BufferedDataReader*
		{
			if (stream == nullptr)
				return nullptr;
			return new BufferedDataReader(stream, filename);
		}));
#endif

		return datareader;
	}

	/*-----------emulating current working directory-----------------*/
	
	const wchar_t * GetCurrentDirConst()
	{
		return g_currentDir->Data();
	}

	wchar_t * GetCurrentDir()//returned pointer should be deleted
	{
		long cwdSize;
		wchar_t *cwd;
		cwdSize = g_currentDir->Length();
		cwd = new wchar_t[cwdSize + 1];

		memcpy(cwd, g_currentDir->Data(), sizeof(wchar_t) * cwdSize);

		cwd[cwdSize] = 0;

		return cwd;
	}
	
	void SetCurrentDir(const wchar_t *dir)
	{
		g_currentDir = (CreateFixedPathString( dir));
	}
	void SetCurrentDir(const char *dir)
	{
		g_currentDir = (CreateFixedPathString(dir));
	}

	/*-----------DataReader class------------------*/
	static const size_t DefaultBufferSize = 8192;

#if USE_WIN32_HANDLE

	DataReader::DataReader(const char *fileName, bool useExactName)
	{
		Platform::String^ refFileName;
		if (!useExactName)
			refFileName = CreateFixedPathString( fileName);
		else{
			int wStringSize = MultiByteToWideChar(CP_UTF8, 0, fileName, -1, NULL, 0);
			wchar_t *wFileName = new wchar_t[wStringSize];
			MultiByteToWideChar(CP_UTF8, 0, fileName, -1, wFileName, wStringSize);
			refFileName = ref new Platform::String(wFileName);
			delete[] wFileName;
		}

		this->fileHandle = OpenFileHandleForRead(refFileName);

		if (this->fileHandle == NULL)
			throw std::bad_alloc();

		FILE_STANDARD_INFO fileInfo = {0};
		if (!GetFileInformationByHandleEx(
			this->fileHandle,
			FileStandardInfo,
			&fileInfo,
			sizeof(fileInfo)) || fileInfo.EndOfFile.HighPart != 0)//currently not support too large file
			throw std::bad_alloc();

		this->totalSize = fileInfo.EndOfFile.LowPart;

		this->currentPointer.QuadPart = 0;

		//copy name
		size_t nameLen = strlen(fileName);
		name = new char[nameLen + 1];
		strncpy(name, fileName, nameLen);
		name[nameLen] = '\0';
		
	}
	DataReader::~DataReader()
	{
		Close();
		delete[] name;
	}


	size_t DataReader::ReadBytes(void* dataOut, size_t elemSize, size_t elemCount)
	{
		size_t bytesRead = ReadBytes((unsigned char*)dataOut, elemSize * elemCount);

		size_t elems = bytesRead / elemSize;

		size_t remain = bytesRead % elemSize;
		//bytesRead is not divisible by elemSize
		if (remain != 0)
		{
			this->Seek(-((long long)remain), SSO_CURRENT);
		}


		return elems;
	}

	size_t DataReader::ReadBytes(unsigned char *ptr, size_t bytes)
	{
		DWORD bytesRead = 0;
		BOOL re = ReadFile(this->fileHandle, ptr, bytes, &bytesRead, NULL);

		if (re != 0)//success
			this->currentPointer.QuadPart += bytesRead;

		return bytesRead;

	}

	int DataReader::GetByte()
	{
		if (this->currentPointer.LowPart >= this->totalSize)
			return EOF;

		char byte;

		if (!ReadBytes((unsigned char *)&byte, 1))
			return EOF;

		return byte;
	}

	int DataReader::Seek(long long offset, StreamSeekOrigin origin)
	{
		size_t position = 0;
		size_t currentPosition = CLAMP_TO_ZERO(Tell());

		LARGE_INTEGER distanceMove;
		distanceMove.QuadPart = offset;

		if (!SetFilePointerEx(this->fileHandle, distanceMove, &this->currentPointer, origin))
			return -1;

		return 0;
	}

	void DataReader::Rewind()
	{
		Seek(0, SSO_BEGIN);
	}

	long long DataReader::Tell() const
	{
		return this->currentPointer.QuadPart;
	}


	int DataReader::Close()
	{
		if (this->fileHandle != nullptr)
		{
			CloseHandle(this->fileHandle);

			this->fileHandle = nullptr;
		}

		return 0;
	}
#else
	//async version

	BufferedDataReader::BufferedDataReader(Windows::Storage::Streams::IRandomAccessStream ^stream, const char* _name)
		: reader(nullptr)
	{
		this->stream = stream;
		
		this->totalSize = stream->Size;

		this->bufferSize = DefaultBufferSize;

		this->buffer = ref new Platform::Array<byte>(this->bufferSize);

		this->ResetStream(0);

		//copy name
		if (_name != NULL)
		{
			size_t nameLen = strlen(_name);
			name = new char[nameLen + 1];
			strncpy(name, _name, nameLen);
			name[nameLen] = '\0';
		}
		else
			name = NULL;
		
	}
	BufferedDataReader::~BufferedDataReader()
	{
		Close();

		if (name != NULL)
			delete[] name;
	}

	bool BufferedDataReader::FillBuffer()
	{
		auto bytesToRead = min(this->totalSize - this->streamOffset, this->bufferSize);
		if (bytesToRead == 0)//no available bytes
			return false;

		auto re = HQWinStoreUtil::Wait(reader->LoadAsync(bytesToRead));

		//copy to buffer
		if (bytesToRead < this->bufferSize)
		{
			//copy to intermediate buffer
			Platform::Array<byte>^ intBuffer = ref new Platform::Array<byte>(bytesToRead);
			reader->ReadBytes(intBuffer);

			memcpy(buffer->Data, intBuffer->Data, bytesToRead);
		}
		else
			reader->ReadBytes(buffer);

		this->unconsumedBufferLength = bytesToRead;
		this->bufferOffset = 0;

		//increase stream offset from the start
		this->streamOffset += bytesToRead;

		return true;
	}

	size_t BufferedDataReader::ReadBytes(void* dataOut, size_t elemSize, size_t elemCount)
	{
		size_t elems = 0;
		unsigned char *ptr = (unsigned char *)dataOut;

		for (size_t i = 0; i < elemCount; ++i)
		{
			size_t availableBytes = this->totalSize - this->streamOffset + this->unconsumedBufferLength;

			ReadBytes(ptr, elemSize);//try to read data to this element

			if (availableBytes < elemSize)//this element can not be read fully
				break;//stop the reading

			elems ++;
			ptr += elemSize;
		}


		return elems;
	}

	void BufferedDataReader::ReadBytes(unsigned char *ptr, size_t bytes)
	{
		size_t bytesToCopy = bytes;

		while(bytesToCopy > 0)
		{
			bool bufferOK = true;
			if (this->unconsumedBufferLength == 0)
			{
				bufferOK = FillBuffer();
			}

			if (bufferOK)
			{
				//calculate the max possible bytes to be copied from buffer 
				size_t maxPossibleBytesToCopy = min(this->unconsumedBufferLength, bytesToCopy);

				//copy from buffer
				memcpy(ptr, buffer->Data + this->bufferOffset, maxPossibleBytesToCopy);

				//decrease the available bytes from buffer and total bytes that are still required
				this->unconsumedBufferLength -= maxPossibleBytesToCopy;
				bytesToCopy -= maxPossibleBytesToCopy;

				//increase the offset on buffer
				this->bufferOffset += maxPossibleBytesToCopy;

				//next block
				ptr += maxPossibleBytesToCopy;
			}
			else
			{
				bytesToCopy = 0;//break
			}
		} 
	}

	int BufferedDataReader::GetByte()
	{
		if (this->streamOffset - this->unconsumedBufferLength >= this->totalSize)
			return EOF;

		char byte;

		ReadBytes((unsigned char *)&byte, 1);

		return byte;
	}

	int BufferedDataReader::Seek(long long offset, StreamSeekOrigin origin)
	{
		size_t position = 0;
		size_t currentPosition = CLAMP_TO_ZERO(Tell());

		switch (origin)
		{
		case SSO_BEGIN:
			position = CLAMP_TO_ZERO(offset);
			break;
		case SSO_CURRENT:
			position = CLAMP_TO_ZERO(currentPosition + offset);
			break;
		case SSO_END:
			if (offset > 0)
				position = this->totalSize;
			else
				position = CLAMP_TO_ZERO(this->totalSize + offset);
			break;
		}

		if (position != currentPosition)
			ResetStream(position);
		return 0;
	}

	void BufferedDataReader::Rewind()
	{
		Seek(0, SSO_BEGIN);
	}

	long long BufferedDataReader::Tell() const
	{
		return this->streamOffset - this->unconsumedBufferLength;
	}

	void BufferedDataReader::ResetStream(size_t position)
	{
		if (this->reader != nullptr)
		{
			reader->DetachStream();
			HQ_DELETE (reader);
			reader = nullptr;
		}

		this->stream->Seek(0);

		this->bufferOffset = 0;

		this->streamOffset = position;

		this->unconsumedBufferLength = 0;

		this->reader = ref new Windows::Storage::Streams::DataReader(stream->GetInputStreamAt(position));

	}

	int BufferedDataReader::Close()
	{
		if (stream != nullptr)
		{
			reader->DetachStream();
			HQ_DELETE (reader);
			delete stream;
			stream = nullptr;
		}

		return 0;
	}

#endif //#if USE_WIN32_HANDLE

};//namespace HQWinStoreFileSystem
