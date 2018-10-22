#pragma once

namespace MultiNES {
	[Windows::Foundation::Metadata::WebHostHidden]
	public delegate void BaseCustomListViewContainerPreparingDelegate(
		Windows::UI::Xaml::DependencyObject^ element,
		Platform::Object^ item
	);

	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class BaseCustomListView : Windows::UI::Xaml::Controls::ListView {
	public:

		event BaseCustomListViewContainerPreparingDelegate^ ItemContainerPreparingHandler;

	internal:
		BaseCustomListView();

	protected:

		virtual void PrepareContainerForItemOverride(
			Windows::UI::Xaml::DependencyObject^ element,
			Platform::Object^ item
		) override;
	};
}