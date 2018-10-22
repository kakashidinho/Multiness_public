//
// GameChooserPage.xaml.cpp
// Implementation of the GameChooserPage class
//

#include "pch.h"
#include "GameChooserPage.xaml.h"
#include "GamePage.xaml.h"

#include <ppl.h>

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
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;
using namespace Concurrency;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=390556

GameChooserPage::GameChooserPage()
{
	InitializeComponent();

	this->manualChooseGameBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::GameChooserPage::OnManualBtnClick);
}

/// <summary>
/// Invoked when this page is about to be displayed in a Frame.
/// </summary>
/// <param name="e">Event data that describes how this page was reached.
/// This parameter is typically used to configure the page.</param>
void GameChooserPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	auto params = dynamic_cast<GameLaunchParams^>(e->Parameter);
	if (params)
		m_gameLauchParams = params;
	else if (m_gameLauchParams == nullptr)
		m_gameLauchParams = ref new GameLaunchParams();//default mode
}

void GameChooserPage::ContinueFileOpenPicker(FileOpenPickerContinuationEventArgs^ args) {
	if (args->Files->Size > 0)
	{
		m_gameLauchParams->GamePath = args->Files->GetAt(0);
		
		Frame->Navigate(TypeName(GamePage::typeid), m_gameLauchParams);
	}
	else {
		//cancel
	}
}


void GameChooserPage::OnManualBtnClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e)
{
	//open dialog to let user choose the game location
	FileOpenPicker^ openPicker = ref new FileOpenPicker();
	openPicker->ViewMode = PickerViewMode::Thumbnail;
	openPicker->SuggestedStartLocation = PickerLocationId::Downloads;
	openPicker->FileTypeFilter->Append(".nes");
	openPicker->FileTypeFilter->Append(".fds");
	openPicker->FileTypeFilter->Append(".nsf");
	openPicker->FileTypeFilter->Append(".unf");
	openPicker->FileTypeFilter->Append(".unif");
	openPicker->FileTypeFilter->Append(".xml");
	openPicker->FileTypeFilter->Append(".zip");

	openPicker->PickSingleFileAndContinue();
}
