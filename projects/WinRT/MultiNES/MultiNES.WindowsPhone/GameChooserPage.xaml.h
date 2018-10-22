//
// GameChooserPage.xaml.h
// Declaration of the GameChooserPage class
//

#pragma once

#include "GameChooserPage.g.h"

#include "GamePage.xaml.h"
#include "ContinuationManager.h"

namespace MultiNES
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class GameChooserPage sealed : public IFileOpenPickerContinuable
	{
	public:
		GameChooserPage();

		//called when filepicker has finished
		virtual void ContinueFileOpenPicker(FileOpenPickerContinuationEventArgs^ args);

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

	private:
		void OnManualBtnClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);

		GameLaunchParams ^m_gameLauchParams;
	};
}
