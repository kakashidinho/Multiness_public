#include "pch.h"
#include "BaseCustomListView.h"

using namespace MultiNES;

BaseCustomListView::BaseCustomListView() 
{}

void BaseCustomListView::PrepareContainerForItemOverride(
	Windows::UI::Xaml::DependencyObject^ element,
	Platform::Object^ item
) {
	ListView::PrepareContainerForItemOverride(element, item);

	ItemContainerPreparingHandler(element, item);
}
