//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"
#include "ContinuationManager.h"

namespace MultiNES
{
		/// <summary>
	/// Provides application-specific behavior to supplement the default Application class.
	/// </summary>
	ref class App sealed
	{
	public:
		App();
		virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;
		virtual void OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ e) override;

	private:
		Windows::UI::Xaml::Controls::Frame^ CreateRootFrame();

		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnResuming(Platform::Object ^sender, Platform::Object ^args);

		void OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e);

		void OnBackPressed(Platform::Object ^sender, Windows::Phone::UI::Input::BackPressedEventArgs ^e);

		concurrency::task<void> RestoreStatus(ApplicationExecutionState previousExecutionState);

		ContinuationManager^ m_continuationManager;
	};
}
