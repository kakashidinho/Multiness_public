//
// DirectXPage.xaml.h
// Declaration of the DirectXPage class.
//

#pragma once

#include "BasePage.h"

#include "GamePage.g.h"

#include "Common\DeviceResources.h"
#include "nestopiaMain.h"

#include <functional>
#include <list>

namespace MultiNES
{
	public enum class GameMode {
		NORMAL_MODE,
		HOST_LAN_GAME_MODE,
		JOIN_LAN_GAME_MODE,
		HOST_INTERNET_GAME_MODE,
		JOIN_INTERNET_GAME_MODE
	};

	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class GameLaunchParams sealed {
	public:
		GameLaunchParams();
		GameLaunchParams(GameMode mode);
		GameLaunchParams(GameLaunchParams ^ src);

		property Windows::Storage::StorageFile^ GamePath;//if this is not null, FilePicker dialog will be used

		property Platform::String^ HostIPOrInviteID;//for JOIN_LAN_GAME_MODE & JOIN_INTERNET_GAME_MODE
		property int HostPort;//for JOIN_LAN_GAME_MODE & HOST_LAN_GAME_MODE. Default is 23458
		property Platform::String^ HostInviteData;//for JOIN_INTERNET_GAME_MODE

		property GameMode Mode {
			GameMode get() {
				return m_mode;
			}

			void set(GameMode mode) {
				m_mode = mode;
			}
		}
	private:
		GameMode m_mode;
	};

	/// <summary>
	/// A page that hosts a DirectX SwapChainPanel.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class GamePage sealed
	{
	public:
		GamePage();
		virtual ~GamePage();

		void SaveInternalState(Windows::Foundation::Collections::IPropertySet^ state);
		void LoadInternalState(Windows::Foundation::Collections::IPropertySet^ state);
	internal:
		//override BasePage's
		virtual void OnBackPressedImpl(Platform::Object ^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^ e) override;

	protected:
		//called when this page becomes the active page
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	private:
		// App event handlers
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnResuming(Platform::Object ^sender, Platform::Object ^args);

		// Window event handlers.
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);

		// DisplayInformation event handlers.
		void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

		// NES event handler
		void OnNesMachineEvent(Nes::Api::Machine::Event event, Nes::Result result);

		// Generic NES result handler
		void HandleGenericNesResult(Nes::Result result, bool errorIsFatal);

		// Other event handlers.
		void AppBarButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel^ sender, Object^ args);
		void OnSwapChainPanelSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		// Track our independent input on a background worker thread.
		Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
		Windows::UI::Core::CoreIndependentInputSource^ m_coreInput;

		// Independent input handling functions.
		void OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);

		void Exit();
		void ShowErrorMessageWithMsgResource(Platform::String^ msgResKey, bool fatal);
		void ShowErrorMessageWithMsgResource(Platform::String^ msgResKey, bool fatal, Windows::UI::Popups::UICommandInvokedHandler^ retryCmd);
		void ShowErrorMessage(Platform::String^ msg, bool fatal);
		void ShowErrorMessage(Platform::String^ msgResKey, bool fatal, Windows::UI::Popups::UICommandInvokedHandler^ retryCmd);

		GameLaunchParams^ m_lauchParams;

		// Resources used to render the DirectX content in the XAML page background.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<nestopiaMain> m_main; 
		bool m_windowVisible;

		//event handlers token
		Windows::Foundation::EventRegistrationToken m_visibilityChangedToken;
		Windows::Foundation::EventRegistrationToken m_suspendingToken;
		Windows::Foundation::EventRegistrationToken m_resumingToken;
		Windows::Foundation::EventRegistrationToken m_dpiChangedToken;
		Windows::Foundation::EventRegistrationToken m_orientationChangedToken;
		Windows::Foundation::EventRegistrationToken m_displayContentsInvalidatedToken;
	};
}

