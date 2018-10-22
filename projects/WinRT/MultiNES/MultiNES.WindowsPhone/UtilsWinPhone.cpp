#include "pch.h"

#include "UtilsWinPhone.h"

namespace MultiNES
{
	void InvokeDefaultOnBackPressedHandler(Windows::UI::Xaml::Controls::Frame^ rootFrame, Windows::Phone::UI::Input::BackPressedEventArgs ^ e)
	{
		if (rootFrame != nullptr)
		{
			e->Handled = true;

			ExitCurrentPage(rootFrame);
		}
	}
}