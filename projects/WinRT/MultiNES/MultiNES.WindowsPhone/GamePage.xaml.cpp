//
// GamePage.xaml.cpp
// Implementation of the GamePage class.
//

#include "pch.h"
#include "GamePage.xaml.h"

#include "Utils.h"

#include <sstream>

using namespace MultiNES;

using namespace Nes;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Popups;
using namespace Windows::Storage;
using namespace concurrency;

#define DEFAULT_LAN_PORT 23458

/*--------- GameLaunchParams --------------*/
GameLaunchParams::GameLaunchParams()
	:m_mode(GameMode::NORMAL_MODE)
{
	this->HostPort = DEFAULT_LAN_PORT;
}

GameLaunchParams::GameLaunchParams(GameMode mode) 
	: m_mode(mode)
{
	this->HostPort = DEFAULT_LAN_PORT;
}

GameLaunchParams::GameLaunchParams(GameLaunchParams ^ src) 
	: m_mode(src->Mode)
{
	this->GamePath = src->GamePath;

	this->HostPort = DEFAULT_LAN_PORT;
}

/*------- GamePage --------------*/
GamePage::GamePage():
	m_windowVisible(true),
	m_coreInput(nullptr),
	m_lauchParams(nullptr)
{
	InitializeComponent();

	// Register event handlers for page lifecycle
	Loaded += ref new RoutedEventHandler(this, &GamePage::OnLoaded);
	Unloaded += ref new RoutedEventHandler(this, &GamePage::OnUnloaded);

	swapChainPanel->CompositionScaleChanged += 
		ref new TypedEventHandler<SwapChainPanel^, Object^>(this, &GamePage::OnCompositionScaleChanged);

	swapChainPanel->SizeChanged +=
		ref new SizeChangedEventHandler(this, &GamePage::OnSwapChainPanelSizeChanged);

	// At this point we have access to the device. 
	// We can create the device-dependent resources.
	m_deviceResources = std::make_shared<DX::DeviceResources>();
	m_deviceResources->SetSwapChainPanel(swapChainPanel);

	// Register our SwapChainPanel to get independent input pointer events
	auto workItemHandler = ref new WorkItemHandler([this] (IAsyncAction ^)
	{
		// The CoreIndependentInputSource will raise pointer events for the specified device types on whichever thread it's created on.
		m_coreInput = swapChainPanel->CreateCoreIndependentInputSource(
			Windows::UI::Core::CoreInputDeviceTypes::Mouse |
			Windows::UI::Core::CoreInputDeviceTypes::Touch |
			Windows::UI::Core::CoreInputDeviceTypes::Pen
			);

		// Register for pointer events, which will be raised on the background thread.
		m_coreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &GamePage::OnPointerPressed);
		m_coreInput->PointerMoved += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &GamePage::OnPointerMoved);
		m_coreInput->PointerReleased += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &GamePage::OnPointerReleased);

		// Begin processing input messages as they're delivered.
		m_coreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
	});

	// Run task on a dedicated high priority background thread.
	m_inputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

	m_main = std::unique_ptr<nestopiaMain>(new nestopiaMain(m_deviceResources));
}

GamePage::~GamePage()
{
	// Stop rendering and processing events on destruction.
	if (m_main)
		m_main->StopRenderLoop();
	m_coreInput->Dispatcher->StopProcessEvents();
}

// Saves the current state of the app for suspend and terminate events.
void GamePage::SaveInternalState(IPropertySet^ state)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->Trim();

	// Stop rendering when the app is suspended.
	m_main->StopRenderLoop();

	// Put code to save app state here.
}

// Loads the current state of the app for resume events.
void GamePage::LoadInternalState(IPropertySet^ state)
{
	// Put code to load app state here.

	// Start rendering when the app is resumed.
	m_main->StartRenderLoop();
}

void GamePage::OnBackPressedImpl(Platform::Object ^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^ e)
{
	e->Handled = true;

	auto resLoader = GetResourceLoader();

	ShowMessage(
		resLoader->GetString("Exit"),
		resLoader->GetString("ExitMsg"),
		ref new UICommandInvokedHandler([this](IUICommand^) {
			Exit();
		}),
		ref new UICommandInvokedHandler([this](IUICommand^) {
			//DO NOTHING
		})
		);
}

void GamePage::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	//register mandatory event handlers
	// Register event handlers for page lifecycle.
	CoreWindow^ window = Window::Current->CoreWindow;

	m_visibilityChangedToken = window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &GamePage::OnVisibilityChanged);

	auto app = Application::Current;
	m_suspendingToken = app->Suspending += ref new SuspendingEventHandler(this, &GamePage::OnSuspending);
	m_resumingToken = app->Resuming += ref new EventHandler<Object^>(this, &GamePage::OnResuming);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_dpiChangedToken = currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &GamePage::OnDpiChanged);

	m_orientationChangedToken = currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &GamePage::OnOrientationChanged);

	m_displayContentsInvalidatedToken = DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &GamePage::OnDisplayContentsInvalidated);

	//set NES's event handler
	m_main->SetNesMachineEventHandler(ref new NesMachineEventHandler(this, &GamePage::OnNesMachineEvent));

	//start game loop
	m_main->StartRenderLoop();

	//parse the launching parameters
	bool loadGame = true;
	switch (m_lauchParams->Mode) {
	case GameMode::NORMAL_MODE:
		break;
	case GameMode::JOIN_LAN_GAME_MODE:
		loadGame = false;//let the host load the game for us
		ShowProgressDialogWithResources(nullptr, "ConnectingToHost", ref new ProgressDialogCancelDelegate([this] {
			Exit();
		}));
		m_main->LoadRemote(m_lauchParams->HostIPOrInviteID, m_lauchParams->HostPort, GetPlayerNameForNetworkPlay());
		break;
	case GameMode::HOST_LAN_GAME_MODE:
		ShowProgressDialogWithResources(nullptr, "ConnectingToMaster", ref new ProgressDialogCancelDelegate([this] {
			Exit();
		}));
		m_main->EnableLanRemoteController(GetPlayerNameForNetworkPlay(), m_lauchParams->HostPort);
		break;
	case GameMode::HOST_INTERNET_GAME_MODE:
		break;
	case GameMode::JOIN_INTERNET_GAME_MODE:
		loadGame = false;//let the host load the game for us
		break;
	}

	if (loadGame) {
		//
		if (m_lauchParams->GamePath)
		{
			m_main->LoadAndStartGame(m_lauchParams->GamePath);
		}
		else {
			//TODO
		}
	}
}

void GamePage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
	auto params = dynamic_cast<GameLaunchParams^>(e->Parameter);
	if (params)
		m_lauchParams = params;
	else if (m_lauchParams == nullptr)
		throw ref new Platform::InvalidArgumentException(L"GameLaunchParams must be passed to GamePage's navigation");
}

// App event handlers
void GamePage::OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e) {
	SaveInternalState(ApplicationData::Current->LocalSettings->Values);
}

void GamePage::OnResuming(Platform::Object ^sender, Platform::Object ^args) {
	LoadInternalState(ApplicationData::Current->LocalSettings->Values);
}

// Window event handlers.

void GamePage::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
	if (m_windowVisible)
	{
		m_main->StartRenderLoop();
	}
	else
	{
		m_main->StopRenderLoop();
	}
}

// DisplayInformation event handlers.

void GamePage::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetDpi(sender->LogicalDpi);
	m_main->CreateWindowSizeDependentResources();
}

void GamePage::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->CreateWindowSizeDependentResources();
}


void GamePage::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->ValidateDevice();
}

void GamePage::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
{
	m_main->OnPointerPressed(sender, e);
}

void GamePage::OnPointerMoved(Object^ sender, PointerEventArgs^ e)
{
	m_main->OnPointerMoved(sender, e);
}

void GamePage::OnPointerReleased(Object^ sender, PointerEventArgs^ e)
{
	m_main->OnPointerReleased(sender, e);
}

void GamePage::OnCompositionScaleChanged(SwapChainPanel^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetCompositionScale(sender->CompositionScaleX, sender->CompositionScaleY);
	m_main->CreateWindowSizeDependentResources();
}

void GamePage::OnSwapChainPanelSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetLogicalSize(e->NewSize);
	m_main->CreateWindowSizeDependentResources();
}

void GamePage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	if (m_main)
	{
		m_main->StopRenderLoop(true);
	}
	
	//unregister event handlers
	Window::Current->CoreWindow->VisibilityChanged -= m_visibilityChangedToken;

	Application::Current->Suspending -= m_suspendingToken;
	Application::Current->Resuming -= m_resumingToken;

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	if (currentDisplayInformation)
	{
		currentDisplayInformation->DpiChanged -= m_dpiChangedToken;

		currentDisplayInformation->OrientationChanged -= m_orientationChangedToken;

		DisplayInformation::DisplayContentsInvalidated -= m_displayContentsInvalidatedToken;
	}
}

// NES event handler
void GamePage::OnNesMachineEvent(Api::Machine::Event event, Result result)
{
	using namespace Api;

	switch (event) {
	case Machine::EVENT_LOAD:
		HandleGenericNesResult(result, true);
		break;
	case Machine::EVENT_LOAD_REMOTE:
		if (NES_FAILED(result)) {
			DismissProgressDialog();
			ShowErrorMessageWithMsgResource("ErrRemoteConn", true);
		}
		break;
	case Machine::EVENT_REMOTE_CONTROLLER_ENABLED:
		if (NES_FAILED(result)) {
			ShowErrorMessageWithMsgResource("ErrRemoteConn", true);
		}
		break;
	case Machine::EVENT_REMOTE_CONNECTED:
	{
		DismissProgressDialog();
		//TODO: show toast
	}
		break;
	case Machine::EVENT_REMOTE_DISCONNECTED:
	{
		DismissProgressDialog();
		ShowErrorMessageWithMsgResource("ErrHostDisconnected", true, ref new UICommandInvokedHandler([this](IUICommand^) {
			//retry
			ShowProgressDialogWithResources(nullptr, "ConnectingToHost", ref new ProgressDialogCancelDelegate([this] {
				Exit();
			}));

			m_main->ReloadRemote();
		}));
	}
		break;
	}//switch (event)
}

void GamePage::HandleGenericNesResult(Nes::Result result, bool errorIsFatal) {
	if (NES_FAILED(result)) {
		auto resLoader = GetResourceLoader();

		String^ error = "";
		switch (result)
		{
		case RESULT_ERR_INVALID_FILE:
			error = resLoader->GetString("ErrInvalidFile");
			break;

		case RESULT_ERR_OUT_OF_MEMORY:
			error = resLoader->GetString("ErrOutOfMem");
			break;

		case RESULT_ERR_CORRUPT_FILE:
			error = resLoader->GetString("ErrCorruptFile");
			break;

		case RESULT_ERR_UNSUPPORTED_MAPPER:
			error = resLoader->GetString("ErrUnsupportedMapper");
			break;

		case RESULT_ERR_MISSING_BIOS:
			error = resLoader->GetString("ErrMissingFdsBios");
			break;

		default:
		{
			std::wostringstream ss;
			ss << result;

			error = resLoader->GetString("Error") + ": " + ref new String(ss.str().c_str());
		}
			break;
		}

		ShowErrorMessage(error, errorIsFatal);
	}//if (NES_FAILED(result))
}

void GamePage::Exit() {
	RunOnUIThread(Dispatcher, [=] {
		if (m_main)
		{
			m_main->StopRenderLoop(true);
		}

		DismissProgressDialog();
		ExitCurrentPage(Frame);
	});
}

void GamePage::ShowErrorMessageWithMsgResource(Platform::String^ msgResKey, bool fatal) {
	ShowErrorMessageWithMsgResource(msgResKey, fatal, nullptr);
}

void GamePage::ShowErrorMessageWithMsgResource(Platform::String^ msgResKey, bool fatal, Windows::UI::Popups::UICommandInvokedHandler^ retryCmd)
{
	ShowErrorMessage(GetResourceLoader()->GetString(msgResKey), fatal, retryCmd);
}

void GamePage::ShowErrorMessage(Platform::String^ msg, bool fatal)
{
	ShowErrorMessage(msg, fatal, nullptr);
}

void GamePage::ShowErrorMessage(Platform::String^ msg, bool fatal, Windows::UI::Popups::UICommandInvokedHandler^ retryCmd)
{
	RunOnUIThread(Dispatcher, [=] {
		DismissProgressDialog();
		auto dismissCmd = ref new UICommandInvokedHandler([this, fatal](IUICommand^) {
			if (fatal)
			{
				Exit();
			}
		});

		if (retryCmd)
		{
			MultiNES::ShowMessage(GetResourceLoader()->GetString("Error"), msg,
				"Cancel",
				dismissCmd,
				"Retry",
				retryCmd
			);
		}
		else {
			MultiNES::ShowErrorMessage(msg, dismissCmd);
		}
	});
}

// Uncomment this if using the app bar in your phone application.
// Called when the app bar button is clicked.
//void GamePage::AppBarButton_Click(Object^ sender, RoutedEventArgs^ e)
//{
//	// Use the app bar if it is appropriate for your app. Design the app bar, 
//	// then fill in event handlers (like this one).
//}
