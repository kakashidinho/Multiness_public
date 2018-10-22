﻿//
// MainPage.xaml.h
// Declaration of the MainPage class
//

#pragma once

#include "MainPage.g.h"

#include "MultipleChoicesDialog.xaml.h"

namespace MultiNES
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class MainPage sealed
	{
	public:
		MainPage();

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		void OnBtnClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);
		void OnMenuFlyoutItemClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);
	};
}
