/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#include "../HQEnginePCH.h"
#include "HQWinStoreUtil.h"
#include "../../HQTimer.h"

#include <new>

namespace HQWinStoreUtil
{
	///condition variable per thread table
	ThreadConditionVarTable::ThreadConditionVarTable()
	{
	}
	ThreadConditionVarTable::~ThreadConditionVarTable()
	{
		for (auto i = this->begin(); i != this->end(); ++i)
		{
			delete i->second;
		}
	}

	ThreadConditionVarTable g_hqengine_cond_vars_internal;

	HQSimpleConditionVar& GetConditionVarForCurrentThread()
	{
		auto threadID = GetCurrentThreadId();
		auto iterator = g_hqengine_cond_vars_internal.find(threadID);
		
		if ( iterator == g_hqengine_cond_vars_internal.end())//not found, create new one
		{
			auto newCond = HQ_NEW HQSimpleConditionVar();

			g_hqengine_cond_vars_internal[threadID] = newCond;

			return *newCond;
		}

		return *iterator->second;
	}

	/*-------------------------------*/
	CStringBuffer::CStringBuffer(size_t _numChars)
		:numChars(_numChars)
	{
		if (numChars != 0)
			cstring = (char*)malloc(numChars + 1);
		if (cstring == NULL)
			throw std::bad_alloc();

		cstring[numChars] = '\0';
	}
	CStringBuffer::~CStringBuffer()
	{
		if (cstring != NULL)
			free(cstring);
	}

	bool CStringBuffer::Resize(size_t numChars)
	{
		if (numChars == 0 && cstring != NULL)
		{
			free(cstring);
			cstring = NULL;
		}
		else
		{
			void *newPtr = realloc(cstring, numChars + 1);
			if (newPtr == NULL)
				return false;

			cstring = (char*)newPtr;
		}

		this->numChars = numChars;
		cstring[numChars] = '\0';

		return true;
	}

	Platform::String^ CreateRefString(const char *cstring)
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

		Platform::String^ refString = ref new Platform::String( wString);
		delete[] wString;

		return refString;
	}


	bool CopyToStringBuffer(Platform::String^ refstring, CStringBuffer &stringOut)
	{
		if (refstring == nullptr)
			return false;

		size_t len = wcstombs(NULL , refstring->Data() , 0);
		if (len != -1)
		{
			try
			{
				if (stringOut.Resize(len) == false)
					throw std::bad_alloc();
				wcstombs(stringOut.c_str() , refstring->Data() , len + 1);
			}
			catch(std::bad_alloc e)
			{
				return false;
			}
		}

		return true;
	}

	/*-------------------------------*/

	void RunOnUIThreadAndWait(Windows::UI::Core::CoreDispatcher^ dispatcher, UIThreadTask task)
	{
		if (dispatcher->HasThreadAccess)//this is ui thread
		{
			//run the task immediately
			task();
		}
		else
		{
			//schedule the task on ui thread and wait for its completion
			Wait(dispatcher->RunAsync(
				Windows::UI::Core::CoreDispatcherPriority::Normal, 
				ref new Windows::UI::Core::DispatchedHandler(
					task,
					Platform::CallbackContext::Any
					)
				));
		}
	}

	void RunOnUIThread(Windows::UI::Core::CoreDispatcher^ dispatcher, UIThreadTask task)
	{
		if (dispatcher->HasThreadAccess)//this is ui thread
		{
			//run the task immediately
			task();
		}
		else
		{
			//schedule the task on ui thread
			dispatcher->RunAsync(
				Windows::UI::Core::CoreDispatcherPriority::Normal, 
				ref new Windows::UI::Core::DispatchedHandler(
					task,
					Platform::CallbackContext::Any
					)
				);
		}
	}
};
