//
// LanServerListingPage.xaml.cpp
// Implementation of the LanServerListingPage class
//

#include "pch.h"
#include "BaseCustomListView.h"
#include "LanServerListingPage.xaml.h"
#include "GamePage.xaml.h"
#include "Utils.h"

#include <codecvt>
#include <string>

using namespace MultiNES;

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Popups;
using namespace Windows::Storage;

static Platform::String^ SETTINGS_CONTAINER = L"LanServerListingPage";
static Platform::String^ SETTING_LAST_USED_SERVER_IP = L"last_ip";

/*---------- LanServer ------------*/
bool LanServer::Equals(Platform::Object^ obj) {
	auto rhs = dynamic_cast<LanServer^> (obj);
	if (rhs == nullptr)
		return false;

	if (rhs == this)
		return true;

	if (Address == nullptr)
		return false;

	return Address->Equals(rhs->Address);
}

int LanServer::GetHashCode() {
	if (Address == nullptr)
		return 0;
	return Address->GetHashCode();
}

struct StringEqual {
	bool operator() (String^ lhs, String^ rhs) const {
		return lhs->Equals(rhs);
	}
};

struct StringHash {
	int operator() (String^ server) const {
		return server->GetHashCode();
	}
};

/*--------- LanServerListingPage::ServerDiscoveryHandler -------------*/
LanServerListingPage::ServerDiscoveryHandler::ServerDiscoveryHandler(LanServerListingPage^ parent) 
	: HQRemote::SocketServerDiscoverClientHandler(this), m_parent(parent)
{
}

// DiscoveryDelegate implementation
void LanServerListingPage::ServerDiscoveryHandler::onNewServerDiscovered(HQRemote::SocketServerDiscoverClientHandler* handler, uint64_t request_id, const char* addr, int reliablePort, int unreliablePort, const char* desc)
{
	//convert strings to wide strings
	Platform::String^ cxAddr = nullptr, ^cxDesc = nullptr;

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	if (addr)
	{
		try {
			auto waddr = converter.from_bytes(addr);
			cxAddr = ref new Platform::String(waddr.c_str());
		}
		catch (...) {
		}
	}//if (addr)

	if (desc)
	{
		try {
			auto wdesc = converter.from_bytes(desc);
			cxDesc = ref new Platform::String(wdesc.c_str());
		}
		catch (...) {
		}
	}//if (desc)

	m_parent->ServerDiscovered(request_id, cxAddr, reliablePort, cxDesc);
}

/*--------------LanServerListingPage ---------*/
LanServerListingPage::LanServerListingPage()
	:m_serverDiscoveryHandler(this)
{
	InitializeComponent();

	Loaded += ref new RoutedEventHandler(this, &LanServerListingPage::OnLoaded);
	Unloaded += ref new RoutedEventHandler(this, &LanServerListingPage::OnUnloaded);

	//register controls' handler
	this->manualBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::LanServerListingPage::OnButtonClick);
	this->joinBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::LanServerListingPage::OnButtonClick);
	this->refreshBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::LanServerListingPage::OnButtonClick);
	this->manualLanDialogJoinBtn->Click += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::LanServerListingPage::OnButtonClick);

	this->manualLanAddressTxtBox->GotFocus += ref new Windows::UI::Xaml::RoutedEventHandler(this, &MultiNES::LanServerListingPage::OnGotFocus);

	//load last typed server's ip
	try {
		auto settings = ApplicationData::Current->LocalSettings;
		auto container = settings->CreateContainer(SETTINGS_CONTAINER, ApplicationDataCreateDisposition::Always);

		auto savedIp = safe_cast<Platform::String^>(container->Values->Lookup(SETTING_LAST_USED_SERVER_IP));
		if (savedIp)
			this->manualLanAddressTxtBox->Text = savedIp;
	}
	catch (...) {

	}

	//refresh server list every 10s
	m_refreshTimer = ref new DispatcherTimer();
	TimeSpan interval;
	interval.Duration = 100000000L;
	m_refreshTimer->Interval = interval;

	m_refreshTimer->Tick += ref new EventHandler<Platform::Object ^>(this, &LanServerListingPage::Refresh);

	//binding data
	m_servers = ref new UnorderedMap<String^, LanServer^, StringHash, StringEqual>();
	m_serversView = ref new Vector<LanServer^>();
	this->listPanel->ItemsSource = m_serversView;

	//selection listener
	this->listPanel->SelectionChanged += ref new Windows::UI::Xaml::Controls::SelectionChangedEventHandler(this, &MultiNES::LanServerListingPage::OnSelectionChanged);

	this->listPanel->ItemContainerPreparingHandler += ref new BaseCustomListViewContainerPreparingDelegate(this, &MultiNES::LanServerListingPage::OnListViewItemContainerPreparing);
}

/// <summary>
/// Invoked when this page is about to be displayed in a Frame.
/// </summary>
/// <param name="e">Event data that describes how this page was reached.
/// This parameter is typically used to configure the page.</param>
void LanServerListingPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	(void) e;	// Unused parameter
}


void LanServerListingPage::Refresh(Platform::Object^ sender, Platform::Object^ args) {
	//disable join button
	this->joinBtn->IsEnabled = false;

	//clear server list
	m_servers->Clear();
	m_serversView->Clear();

	//start finding servers
	m_serverDiscoveryHandler.findOtherServers(0);
}

void LanServerListingPage::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	//register event handlers
	auto app = Application::Current;
	m_suspendingToken = app->Suspending += ref new SuspendingEventHandler(this, &LanServerListingPage::OnSuspending);
	m_resumingToken = app->Resuming += ref new EventHandler<Object^>(this, &LanServerListingPage::OnResuming);

	StartServerDiscovering();
}


void LanServerListingPage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	//unregister event handlers
	auto app = Application::Current;
	app->Suspending -= m_suspendingToken;
	app->Resuming -= m_resumingToken;

	StopServerDiscovering();
}

void LanServerListingPage::OnResuming(Platform::Object ^sender, Platform::Object ^args) {
	StartServerDiscovering();
}

void LanServerListingPage::OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e) {
	StopServerDiscovering();
}

//server discovery
void LanServerListingPage::ServerDiscovered(uint64_t request_id, Platform::String^ addr, int port, Platform::String^ desc)
{
	Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
		ref new Windows::UI::Core::DispatchedHandler([this, request_id, addr, port, desc] {
		auto server = ref new LanServer();
		server->Address = addr;
		server->Port = port;
		if (desc)
			server->Desc = desc;
		else
			server->Desc = addr;

		if (!m_servers->Insert(server->Address, server))
		{
			m_serversView->Append(server);

			if (server->Equals(m_selectedServer))
				listPanel->SelectedIndex = m_servers->Size - 1;
		}
	}));
}

void LanServerListingPage::StartServerDiscovering()
{
	//start discovery service
	m_serverDiscoveryHandler.start();

	//start refreshing repeatedly
	if (!m_refreshTimer->IsEnabled)
	{
		m_refreshTimer->Start();

		//first refresh
		Refresh(nullptr, nullptr);
	}
}
void LanServerListingPage::StopServerDiscovering()
{
	//stop refreshing
	if (m_refreshTimer->IsEnabled)
		m_refreshTimer->Stop();

	//stop discovery service
	m_serverDiscoveryHandler.stop();
}

void LanServerListingPage::OnSelectionChanged(Platform::Object ^sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs ^e)
{
	for (auto selected : e->AddedItems) {
		//enable join button
		this->joinBtn->IsEnabled = true;

		//cache selected server
		m_selectedServer = dynamic_cast<LanServer^> ((Object^)selected);

		//highlight the selected server
		auto container = this->listPanel->ContainerFromItem(selected);

		HighlightItem(container, true);
	}

	for (auto deselected : e->RemovedItems) {
		//unhighlight the selected server
		auto container = this->listPanel->ContainerFromItem(deselected);
		
		HighlightItem(container, false);
	}

	return;
}


void LanServerListingPage::OnButtonClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e)
{
	auto button = dynamic_cast<Button^>(sender);
	if (button == this->manualBtn) {
		//already handled by flyout 
	}
	else if (button == this->joinBtn) {
		if (m_selectedServer)
		{
			GameLaunchParams^ params = ref new GameLaunchParams(GameMode::JOIN_LAN_GAME_MODE);
			params->HostIPOrInviteID = m_selectedServer->Address;
			params->HostPort = m_selectedServer->Port;

			Frame->Navigate(TypeName(GamePage::typeid), params);
		}
	}
	else if (button == this->refreshBtn) {
		Refresh(nullptr, nullptr);
	}
	else if (button == this->manualLanDialogJoinBtn) {
		auto server_ip = this->manualLanAddressTxtBox->Text;
		if (server_ip == nullptr || server_ip->Length() == 0)
		{
			//error
			ShowErrorMessage(GetResourceLoader()->GetString("ErrInvalidIp"), ref new UICommandInvokedHandler([this](IUICommand^ cmd) {
				//show the manual address dialog again
				manualLanDialog->ShowAt(manualBtn);
			}));
		}
		else {
			//save the entered ip for later reuse
			try {
				auto settings = ApplicationData::Current->LocalSettings;
				auto container = settings->CreateContainer(SETTINGS_CONTAINER, ApplicationDataCreateDisposition::Always);

				container->Values->Insert(SETTING_LAST_USED_SERVER_IP, dynamic_cast<PropertyValue^>(PropertyValue::CreateString(server_ip)));
			}
			catch (...) {

			}

			//enter the game
			GameLaunchParams^ params = ref new GameLaunchParams(GameMode::JOIN_LAN_GAME_MODE);
			params->HostIPOrInviteID = server_ip;

			Frame->Navigate(TypeName(GamePage::typeid), params);
		}
	}//else if (button = this->manualLanDialogJoinBtn)
}

void LanServerListingPage::HighlightItem(Windows::UI::Xaml::DependencyObject^ element, bool highlight) {
	auto frame = FindXamlChild<StackPanel>(element, L"ItemContainer");
	if (frame) {
		if (highlight)
			frame->Background = ref new SolidColorBrush(Windows::UI::Colors::DimGray);
		else
			frame->Background = nullptr;
	}
}


void MultiNES::LanServerListingPage::OnListViewItemContainerPreparing(Windows::UI::Xaml::DependencyObject ^element, Platform::Object ^item)
{
	if (m_selectedServer != nullptr && m_selectedServer->Equals(item))
	{
		HighlightItem(element, true);
	}
	else
		HighlightItem(element, false);
}


void LanServerListingPage::OnGotFocus(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e)
{
	auto textBox = dynamic_cast<TextBox^>(sender);
	if (textBox != nullptr) {
		if (textBox == this->manualLanAddressTxtBox)
			textBox->SelectAll();
	}
}
