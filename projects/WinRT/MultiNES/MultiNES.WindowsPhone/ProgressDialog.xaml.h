//
// ProgressDialog.xaml.h
// Declaration of the ProgressDialog class
//

#pragma once

#include "ProgressDialog.g.h"

namespace MultiNES
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public delegate void ProgressDialogCancelDelegate();

	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class ProgressDialog sealed
	{
	public:
		ProgressDialog();

		property ProgressDialogCancelDelegate^ CancelDelegate;
		property Platform::String^ Message {
			void set(Platform::String^ msg) {
				this->message->Text = msg;
			}
		}
	private:
		void ContentDialog_PrimaryButtonClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args);
		void OnClosed(Windows::UI::Xaml::Controls::ContentDialog ^sender, Windows::UI::Xaml::Controls::ContentDialogClosedEventArgs ^args);
	};
}
