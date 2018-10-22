#pragma once

#include "ProgressDialog.xaml.h"

namespace MultiNES {
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class BasePage : Windows::UI::Xaml::Controls::Page {
	public:
		void OnBackPressed(Platform::Object ^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^ e);
	internal:
		//TODO: wtf why MS doesn't allow public method to be virtual ???
		virtual void OnBackPressedImpl(Platform::Object ^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^ e);

	protected:
		void DismissProgressDialog();
		void ShowProgressDialog(Platform::String^ title, Platform::String^ msg, ProgressDialogCancelDelegate^ cancelHandler);
		void ShowProgressDialogWithResources(Platform::String^ titleResKey, Platform::String^ msgResKey, ProgressDialogCancelDelegate^ cancelHandler);

	private:
		ProgressDialog^ m_progressDialog;
		Windows::Foundation::IAsyncOperation<Windows::UI::Xaml::Controls::ContentDialogResult>^ m_progressDialogShowOp;
	};
}