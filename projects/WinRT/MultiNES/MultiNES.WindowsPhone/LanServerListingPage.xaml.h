//
// LanServerListingPage.xaml.h
// Declaration of the LanServerListingPage class
//

#pragma once

#include "LanServerListingPage.g.h"

#include "BaseCustomListView.h"
#include "RemoteController/ConnectionHandler.h"

namespace MultiNES
{
	[Windows::UI::Xaml::Data::Bindable]
	public ref class LanServer sealed {
	public:
		property Platform::String^ Address;
		property int Port;
		property Platform::String^ Desc;

		property Platform::String^ AddressStr {
			Platform::String^ get() {
				return Address + ":" + Port;
			}
		}

		bool Equals(Platform::Object^ obj);
		int GetHashCode();
	};

	//custom listview to make our server item highlightable
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class LanServerListView sealed : public BaseCustomListView {
	public:
		LanServerListView()
		{}
	};

	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class LanServerListingPage sealed
	{
	public:
		LanServerListingPage();

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

	private:
		void OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnResuming(Platform::Object ^sender, Platform::Object ^args);

		void OnSelectionChanged(Platform::Object ^sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs ^e);
		void OnButtonClick(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);

		void OnListViewItemContainerPreparing(Windows::UI::Xaml::DependencyObject ^element, Platform::Object ^item);

		void HighlightItem(Windows::UI::Xaml::DependencyObject^ element, bool highlight);

		void Refresh(Platform::Object^ sender, Platform::Object^ args);

		void ServerDiscovered(uint64_t request_id, Platform::String^ addr, int port, Platform::String^ desc);
		void StartServerDiscovering();
		void StopServerDiscovering();

		Windows::UI::Xaml::DispatcherTimer^ m_refreshTimer;

		//event handlers token
		Windows::Foundation::EventRegistrationToken m_suspendingToken;
		Windows::Foundation::EventRegistrationToken m_resumingToken;

		//server discovery
		struct ServerDiscoveryHandler : public HQRemote::SocketServerDiscoverClientHandler, public HQRemote::SocketServerDiscoverClientHandler::DiscoveryDelegate
		{
			ServerDiscoveryHandler(LanServerListingPage^ parent);

			// DiscoveryDelegate implementation
			virtual void onNewServerDiscovered(HQRemote::SocketServerDiscoverClientHandler* handler, uint64_t request_id, const char* addr, int reliablePort, int unreliablePort, const char* desc) override;

		private:
			LanServerListingPage^ m_parent;
		};

		ServerDiscoveryHandler m_serverDiscoveryHandler;
		Windows::Foundation::Collections::IMap<Platform::String^, LanServer^>^ m_servers;
		Platform::Collections::Vector<LanServer^>^ m_serversView;
		LanServer^ m_selectedServer;
		void OnGotFocus(Platform::Object ^sender, Windows::UI::Xaml::RoutedEventArgs ^e);
	};
}
