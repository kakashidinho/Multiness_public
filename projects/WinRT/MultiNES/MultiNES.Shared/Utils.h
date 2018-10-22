#pragma once

#include <functional>

namespace MultiNES {
	Platform::String^ GetPlayerNameForNetworkPlay();

	void ExitCurrentPage(Windows::UI::Xaml::Controls::Frame^ rootFrame);

	Windows::ApplicationModel::Resources::ResourceLoader^ GetResourceLoader();

	void ShowErrorMessage(
		Platform::String^ message,
		Windows::UI::Popups::UICommandInvokedHandler ^ okCommand);

	void ShowMessage(
		Platform::String^ title,
		Platform::String^ message,
		Windows::UI::Popups::UICommandInvokedHandler ^ okCommand);

	void ShowMessage(
		Platform::String^ title,
		Platform::String^ message,
		Windows::UI::Popups::UICommandInvokedHandler ^ okCommand,
		Windows::UI::Popups::UICommandInvokedHandler ^ cancelCommand);

	void ShowMessage(
		Platform::String^ title,
		Platform::String^ message,
		Platform::String^ okLabelResKey,
		Windows::UI::Popups::UICommandInvokedHandler ^ okCommand,
		Platform::String^ cancelLabelResKey,
		Windows::UI::Popups::UICommandInvokedHandler ^ cancelCommand);

	void RunOnUIThread(Windows::UI::Core::CoreDispatcher^ dispatcher, std::function<void()> task);

	//this one I got from StackOverflow
	template <class T>
	T^ FindXamlChild(Windows::UI::Xaml::DependencyObject^ parent, Platform::String^ childName)
	{
		// Confirm parent and childName are valid. 
		if (parent == nullptr) return nullptr;

		T^ foundChild = nullptr;

		auto childrenCount = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(parent);
		for (int i = 0; i < childrenCount; i++)
		{
			auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(parent, i);
			// If the child is not of the request child type child
			auto childType = dynamic_cast<T^>(child);
			if (childType == nullptr)
			{
				// recursively drill down the tree
				foundChild = FindXamlChild<T>(child, childName);

				// If the child is found, break so we do not overwrite the found child. 
				if (foundChild != nullptr) break;
			}
			else if (childName != nullptr && childName->Length() > 0)
			{
				auto frameworkElement = dynamic_cast<Windows::UI::Xaml::FrameworkElement^>(child);
				// If the child's name is set for search
				if (frameworkElement != nullptr && frameworkElement->Name == childName)
				{
					// if the child's name is of the request name
					foundChild = static_cast<T^>(child);
					break;
				}
			}
			else
			{
				// child element found.
				foundChild = static_cast<T^>(child);
				break;
			}
		}

		return foundChild;
	}
}
