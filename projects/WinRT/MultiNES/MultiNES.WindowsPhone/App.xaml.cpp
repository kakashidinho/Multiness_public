//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include "MainPage.xaml.h"

#include "UtilsWinPhone.h"
#include "BasePage.h"

using namespace MultiNES;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Phone::UI::Input;

/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
	InitializeComponent();
	Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
	Resuming += ref new EventHandler<Object^>(this, &App::OnResuming);
	HardwareButtons::BackPressed += ref new EventHandler<BackPressedEventArgs^>(this, &App::OnBackPressed);
}

Windows::UI::Xaml::Controls::Frame^ App::CreateRootFrame() {
	Frame^ rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

	if (rootFrame == nullptr)
	{
		// Create a Frame to act as the navigation context  
		rootFrame = ref new Frame();

		// Set the default language 
		rootFrame->Language = Windows::Globalization::ApplicationLanguages::Languages->GetAt(0);
		rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);

		// Place the frame in the current Window 
		Window::Current->Content = rootFrame;
	}

	return rootFrame;
}

concurrency::task<void> App::RestoreStatus(ApplicationExecutionState previousExecutionState) {
	auto prerequisite = Concurrency::task<void>([]() {});
	if (previousExecutionState == ApplicationExecutionState::Terminated)
	{
		//TODO
	}

	return prerequisite;
}

/// <summary>
/// Invoked when the application is launched normally by the end user.  Other entry points
/// will be used when the application is launched to open a specific file, to display
/// search results, and so forth.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
#if _DEBUG
	if (IsDebuggerPresent())
	{
		DebugSettings->EnableFrameRateCounter = true;
	}
#endif

	auto rootFrame = CreateRootFrame();
	RestoreStatus(e->PreviousExecutionState).then([=](Concurrency::task<void> prerequisite)
	{
		try
		{
			prerequisite.get();
		}
		catch (Platform::Exception^)
		{
			//Something went wrong restoring state. 
			//Assume there is no state and continue 
		}

		//MainPage is always in rootFrame so we don't have to worry about restoring the navigation state on resume 
		rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);


		// Ensure the current window is active 
		Window::Current->Activate();
	}, Concurrency::task_continuation_context::use_current());
}

void App::OnActivated(Windows::ApplicationModel::Activation::IActivatedEventArgs^ e) {
	Application::OnActivated(e);

	m_continuationManager = ref new MultiNES::ContinuationManager();

	auto rootFrame = CreateRootFrame();

	RestoreStatus(e->PreviousExecutionState).then([=](Concurrency::task<void> prerequisite)
	{
		try
		{
			prerequisite.get();
		}
		catch (Platform::Exception^)
		{
			//Something went wrong restoring state. 
			//Assume there is no state and continue 
		}

		if (rootFrame->Content == nullptr)
		{
			rootFrame->Navigate(TypeName(MainPage::typeid));
		}

		auto continuationEventArgs = dynamic_cast<IContinuationActivatedEventArgs^>(e);
		if (continuationEventArgs != nullptr)
		{
			// Call ContinuationManager to handle continuation activation 
			m_continuationManager->Continue(continuationEventArgs, rootFrame);
		}

		Window::Current->Activate();
	}, Concurrency::task_continuation_context::use_current());
}

/// <summary>
/// Invoked when application execution is being suspended.  Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
	(void) sender;	// Unused parameter
	(void) e;
}

/// <summary>
/// Invoked when application execution is being resumed.
/// </summary>
/// <param name="sender">The source of the resume request.</param>
/// <param name="args">Details about the resume request.</param>
void App::OnResuming(Object ^sender, Object ^args)
{
	(void) sender; // Unused parameter
	(void) args; // Unused parameter
}

/// <summary> 
/// Invoked when Navigation to a certain page fails 
/// </summary> 
/// <param name="sender">The Frame which failed navigation</param> 
/// <param name="e">Details about the navigation failure</param> 
void App::OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e)
{
	throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}

void App::OnBackPressed(Platform::Object ^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^ e)
{
	Frame^ rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

	if (rootFrame) {
		auto page = dynamic_cast<BasePage^>(rootFrame->Content);
		if (page)
			page->OnBackPressed(sender, e);
		else
			InvokeDefaultOnBackPressedHandler(rootFrame, e);
	}
}
