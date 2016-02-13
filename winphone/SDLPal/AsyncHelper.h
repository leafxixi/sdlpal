/*
*   AsyncHelper.h
*
*   Date: 1st July, 2014   Author: David Huang
*   (C) 2014 Light Studio. All Rights Reserved.
*/
#pragma once
#include <ppltasks.h>
#include <Windows.h>
#include <synchapi.h>
using namespace Platform;

template <typename TResult>
inline TResult AWait(Windows::Foundation::IAsyncOperation<TResult>^ asyncOp)
{
	HANDLE handle = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	TResult result;
	Exception^ exceptionObj = nullptr;
	asyncOp->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<TResult>([&]
		(Windows::Foundation::IAsyncOperation<TResult>^ asyncInfo, Windows::Foundation::AsyncStatus asyncStatus)
	{
		try
		{
			result = asyncInfo->GetResults();
		}
		catch (Exception^ e)
		{
			exceptionObj = e;
		}
		SetEvent(handle);
	});
	WaitForSingleObjectEx(handle, INFINITE, FALSE);
	if (exceptionObj != nullptr) throw exceptionObj;
	return result;
}

inline void AWait(Windows::Foundation::IAsyncAction^ asyncAc)
{
	HANDLE handle = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
	Exception^ exceptionObj = nullptr;
	asyncAc->Completed = ref new Windows::Foundation::AsyncActionCompletedHandler([&]
		(Windows::Foundation::IAsyncAction^ asyncInfo, Windows::Foundation::AsyncStatus asyncStatus)
	{
		try
		{
			asyncInfo->GetResults();
		}
		catch (Exception^ e)
		{
			exceptionObj = e;
		}
		SetEvent(handle);
	});
	WaitForSingleObjectEx(handle, INFINITE, FALSE);
	if (exceptionObj != nullptr) throw exceptionObj;
}