//
// MainPage.xaml.cpp
// Implementation of the MainPage class
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "GameChooserPage.xaml.h"
#include "MultipleChoicesDialog.xaml.h"
#include "LanServerListingPage.xaml.h"

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
using namespace Windows::UI::Xaml::Interop;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=390556

MainPage::MainPage()
{
	InitializeComponent();

	this->multiplayerLanBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::MainPage::OnBtnClick);
	this->singleplayerBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::MainPage::OnBtnClick);

	this->lanP1HostBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::MainPage::OnMenuFlyoutItemClick);
	this->lanP2GuestBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::MainPage::OnMenuFlyoutItemClick);
}

/// <summary>
/// Invoked when this page is about to be displayed in a Frame.
/// </summary>
/// <param name="e">Event data that describes how this page was reached.
/// This parameter is typically used to configure the page.</param>
void MainPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	(void) e;	// Unused parameter
}


void MainPage::OnBtnClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e)
{
	auto button = dynamic_cast<Button^>(sender);
	if (button == nullptr)
		return;

	if (button == this->multiplayerLanBtn) {
		
	}//if (button == this->multiplayerLanBtn)
	else if (button == this->singleplayerBtn) {
		Frame->Navigate(TypeName(GameChooserPage::typeid));
	}
}

void MainPage::OnMenuFlyoutItemClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e) {
	auto item = dynamic_cast<RadioButton^> (sender);
	if (!item)
		return;

	if (item == this->lanP1HostBtn) {
		this->multiplayerLanFlyout->Hide();

		//host lan game
		GameLaunchParams^ params = ref new GameLaunchParams(GameMode::HOST_LAN_GAME_MODE);

		Frame->Navigate(TypeName(GameChooserPage::typeid), params);

	} else if (item == this->lanP2GuestBtn) {
		this->multiplayerLanFlyout->Hide();

		//join lan game
		Frame->Navigate(TypeName(LanServerListingPage::typeid));
	}
}
