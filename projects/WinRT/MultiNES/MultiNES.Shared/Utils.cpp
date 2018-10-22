#include "pch.h"
#include "Utils.h"

using namespace Windows::UI::Popups;

namespace MultiNES {
	Platform::String^ GetPlayerNameForNetworkPlay() {
		//TODO
		return "A player";
	}
	
	void ExitCurrentPage(Windows::UI::Xaml::Controls::Frame^ rootFrame)
	{
		if (rootFrame != nullptr && rootFrame->CanGoBack) {
			rootFrame->GoBack();

			//disable forward going
			auto forwardStack = rootFrame->ForwardStack;
			if (forwardStack && forwardStack->Size > 0)
			{
#ifdef _DEBUG
				auto top = forwardStack->GetAt(forwardStack->Size - 1);//debugging
				auto topType = top->SourcePageType;
#endif
				forwardStack->Clear();
			}
		}
		else
			Windows::UI::Xaml::Application::Current->Exit();
	}

	void RunOnUIThread(Windows::UI::Core::CoreDispatcher^ dispatcher, std::function<void()> task) {
		if (dispatcher->HasThreadAccess)
			task();
		else
			dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler(task));
	}

	Windows::ApplicationModel::Resources::ResourceLoader^ GetResourceLoader() {
		return Windows::ApplicationModel::Resources::ResourceLoader::GetForCurrentView();
	}

	void ShowErrorMessage(
		Platform::String^ message,
		Windows::UI::Popups::UICommandInvokedHandler ^ okCommand)
	{
		ShowMessage(GetResourceLoader()->GetString("Error"), message, okCommand);
	}

	void ShowMessage(
		Platform::String^ title,
		Platform::String^ message,
		Windows::UI::Popups::UICommandInvokedHandler ^ okCommand) {
		ShowMessage(title, message, "OK", okCommand, nullptr, nullptr);
	}

	void ShowMessage(
		Platform::String^ title,
		Platform::String^ message,
		Windows::UI::Popups::UICommandInvokedHandler ^ okCommand,
		Windows::UI::Popups::UICommandInvokedHandler ^ cancelCommand) {
		ShowMessage(title, message, "OK", okCommand, "Cancel", cancelCommand);
	}

	void ShowMessage(
		Platform::String^ title,
		Platform::String^ message,
		Platform::String^ okLabelResKey,
		Windows::UI::Popups::UICommandInvokedHandler ^ okHandler,
		Platform::String^ cancelLabelResKey,
		Windows::UI::Popups::UICommandInvokedHandler ^ cancelHandler) {

		auto resLoader = GetResourceLoader();

		auto dialog = ref new MessageDialog(message, title);

		auto okCmd = ref new UICommand(resLoader->GetString(okLabelResKey));
		okCmd->Invoked = okHandler;
		dialog->Commands->Append(okCmd);

		if (cancelHandler) {
			auto cancelCmd = ref new UICommand(resLoader->GetString(cancelLabelResKey));
			cancelCmd->Invoked = cancelHandler;
			dialog->Commands->Append(cancelCmd);
		}

		dialog->CancelCommandIndex = dialog->Commands->Size - 1;

		dialog->ShowAsync();
	}
}