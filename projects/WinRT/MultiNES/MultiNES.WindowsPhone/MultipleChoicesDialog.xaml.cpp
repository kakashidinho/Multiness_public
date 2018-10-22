//
// MultipleChoicesDialog.xaml.cpp
// Implementation of the MultipleChoicesDialog class
//

#include "pch.h"
#include "MultipleChoicesDialog.xaml.h"

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

// The Content Dialog item template is documented at http://go.microsoft.com/fwlink/?LinkID=390556

MultiNES::MultipleChoicesDialog::MultipleChoicesDialog()
{
	InitializeComponent();

	//remove all sample radio buttons
	this->root->Children->Clear();
}

uint32 MultipleChoicesDialog::NumChoices() {
	return this->root->Children->Size;
}

void MultipleChoicesDialog::AddChoice(Platform::String^ description) {
	auto radioButton = ref new RadioButton();
	radioButton->Content = description;
	radioButton->Tag = ref new Platform::Box<uint32>(NumChoices());

	radioButton->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::MultipleChoicesDialog::OnClick);

	this->root->Children->Append(radioButton);
}


void MultipleChoicesDialog::OnClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e)
{
	auto button = dynamic_cast<RadioButton^>(sender);
	if (button == nullptr || button->Tag == nullptr)
		return;

	auto index = safe_cast<uint32>(button->Tag);

	SelectedHandler(this, index);
}
