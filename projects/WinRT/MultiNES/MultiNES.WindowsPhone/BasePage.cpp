#include "pch.h"

#include "BasePage.h"
#include "UtilsWinPhone.h"

using namespace MultiNES;

void BasePage::OnBackPressed(Platform::Object ^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^ e) {
	OnBackPressedImpl(sender, e);
}

void BasePage::OnBackPressedImpl(Platform::Object ^ sender, Windows::Phone::UI::Input::BackPressedEventArgs ^ e) {
	InvokeDefaultOnBackPressedHandler(Frame, e);
}

void BasePage::DismissProgressDialog() {
	RunOnUIThread(Dispatcher, [&] {
		if (m_progressDialog)
		{
			m_progressDialog->CancelDelegate = nullptr;
			m_progressDialog->Hide();
		}
		if (m_progressDialogShowOp)
			m_progressDialogShowOp->Cancel();

		m_progressDialogShowOp = nullptr;
		m_progressDialog = nullptr;
	});
}

void BasePage::ShowProgressDialogWithResources(Platform::String^ titleResKey, Platform::String^ msgResKey, ProgressDialogCancelDelegate^ cancelHandler)
{
	RunOnUIThread(Dispatcher, [&] {
		auto resLoader = GetResourceLoader();
		ShowProgressDialog(titleResKey != nullptr ? resLoader->GetString(titleResKey) : "",
			msgResKey != nullptr ? resLoader->GetString(msgResKey) : "",
			cancelHandler);
	});
}

void BasePage::ShowProgressDialog(Platform::String^ title, Platform::String^ msg, ProgressDialogCancelDelegate^ cancelHandler) {
	RunOnUIThread(Dispatcher, [&] {
		DismissProgressDialog();

		if (!m_progressDialog)
			m_progressDialog = ref new ProgressDialog();

		m_progressDialog->Title = title;
		m_progressDialog->Message = msg;
		m_progressDialog->CancelDelegate = cancelHandler;

		m_progressDialogShowOp = m_progressDialog->ShowAsync();
	});
}