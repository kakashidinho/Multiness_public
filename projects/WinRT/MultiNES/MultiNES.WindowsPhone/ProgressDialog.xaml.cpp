//
// ProgressDialog.xaml.cpp
// Implementation of the ProgressDialog class
//

#include "pch.h"
#include "ProgressDialog.xaml.h"

using namespace MultiNES;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::System;

// The Content Dialog item template is documented at http://go.microsoft.com/fwlink/?LinkID=390556

MultiNES::ProgressDialog::ProgressDialog()
{
	InitializeComponent();

	this->Closed += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Xaml::Controls::ContentDialog ^, Windows::UI::Xaml::Controls::ContentDialogClosedEventArgs ^>(this, &MultiNES::ProgressDialog::OnClosed);
}

void ProgressDialog::ContentDialog_PrimaryButtonClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args)
{
}


void ProgressDialog::OnClosed(Windows::UI::Xaml::Controls::ContentDialog ^sender, Windows::UI::Xaml::Controls::ContentDialogClosedEventArgs ^args)
{
	switch (args->Result) {
	case ContentDialogResult::None:
	case ContentDialogResult::Primary:
		if (CancelDelegate)
			CancelDelegate->Invoke();
		break;
	}
}
