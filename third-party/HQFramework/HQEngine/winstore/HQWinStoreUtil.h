/*
Copyright (C) 2010-2013  Le Hoang Quyen (lehoangq@gmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the MIT license.  See the file
COPYING.txt included with this distribution for more information.


*/

#pragma once

#include "../../HQTimer.h"
#include "../../HQConditionVariable.h"

#include <ppltasks.h>
#include <unordered_map>

namespace HQWinStoreUtil
{
	///
	///holding condition variable of each thread
	///
	class ThreadConditionVarTable: public std::unordered_map<DWORD, HQSimpleConditionVar*>
	{
	public:
		ThreadConditionVarTable();
		~ThreadConditionVarTable();
	};

	extern ThreadConditionVarTable g_hqengine_cond_vars_internal;

	HQSimpleConditionVar& GetConditionVarForCurrentThread();

	class CStringBuffer
	{
	public:
		CStringBuffer(size_t numChars);
		~CStringBuffer();

		bool Resize(size_t numChars);
		char *c_str() {return cstring;}
		const char *c_str() const {return cstring;}
		size_t size() const {return numChars;}
	private:
		char *cstring;
		size_t numChars;
	};

	Platform::String^ CreateRefString(const char *cstring);
	bool CopyToStringBuffer(Platform::String^ refstring, CStringBuffer &stringOut);

	
	template <class T> inline T Wait(Concurrency::task<T>& task)
	{
		using namespace Concurrency;
		
		HQSimpleConditionVar& condVar = GetConditionVarForCurrentThread();
		T result;


		condVar.Lock();

		auto waitTask = create_task([&] (){

			condVar.Lock();

			try
			{
				result = task.get();//wait for task
			}
			catch (...)
			{
				result = T();//default value
			}
			condVar.Signal();//signal done 

			condVar.Unlock();
		});
		
		

		condVar.Wait();//wait for task
		condVar.Unlock();

		return result;
	}

	template <> inline void Wait(Concurrency::task<void>& task)
	{
		using namespace Concurrency;
		
		HQSimpleConditionVar& condVar = GetConditionVarForCurrentThread();

		condVar.Lock();

		auto waitTask = create_task([&] (){
			condVar.Lock();
			try
			{
				task.get();//wait for task
			}
			catch (...)
			{
			}

			condVar.Signal();//signal done 

			condVar.Unlock();
		});

		condVar.Wait();//wait for task
		condVar.Unlock();
	}

	typedef std::function<void (void)> UIThreadTask;
	//this method will not block if current thread is UI thread
	void RunOnUIThreadAndWait(Windows::UI::Core::CoreDispatcher^ dispatcher, UIThreadTask task);
	void RunOnUIThread(Windows::UI::Core::CoreDispatcher^ dispatcher, UIThreadTask task);

	inline void Wait(Windows::Foundation::IAsyncAction^ task)
	{
		using namespace Windows::Storage;
		using namespace Windows::Foundation;
		using namespace Concurrency;
		
		//wait for task
		Wait(create_task(task));
	}

	template <class T> inline T Wait(Windows::Foundation::IAsyncOperation<T>^ task)
	{
		using namespace Windows::Storage;
		using namespace Windows::Foundation;
		using namespace Concurrency;

		//wait for task
		try
		{
			return Wait(create_task(task));
		}
		catch (...)
		{
			return T();
		}
	}

	template <class Result, class Progress> inline Result Wait(Windows::Foundation::IAsyncOperationWithProgress<Result, Progress>^ task)
	{
		using namespace Windows::Storage;
		using namespace Windows::Foundation;
		using namespace Concurrency;

		//wait for task
		try
		{
			return Wait(create_task(task));
		}
		catch (...)
		{
			return T();
		}
	}
};
