//
// MultipleChoicesDialog.xaml.h
// Declaration of the MultipleChoicesDialog class
//

#pragma once

#include "MultipleChoicesDialog.g.h"

namespace MultiNES
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public delegate void ChoiceSelectedHandler(MultipleChoicesDialog^ parentDialog, uint32 selectedIdx);

	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class MultipleChoicesDialog sealed
	{
	public:
		MultipleChoicesDialog();

		uint32 NumChoices();

		void AddChoice(Platform::String^ description);

		event  ChoiceSelectedHandler^ SelectedHandler;
	private:
		void OnClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);
	};
}
