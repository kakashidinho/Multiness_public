////////////////////////////////////////////////////////////////////////////////////////
//
// Multiness - NES/Famicom emulator written in C++
// Based on Nestopia emulator
//
// Copyright (C) 2016-2018 Le Hoang Quyen
//
// This file is part of Multiness.
//
// Multiness is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Multiness is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Multiness; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef ConnectionHandlerRakNet_hpp
#define ConnectionHandlerRakNet_hpp

#include <stdio.h>

#include "RakPeerInterface.h"
#include "NatPunchthroughClient.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"

#include "../../third-party/RemoteController/ConnectionHandler.h"

#include <memory>
#include <thread>
#include <mutex>
#include <functional>
#include <unordered_set>
#include <string>
#include <list>

namespace Nes {
	namespace Remote {
		class ConnectionHandlerRakNet: public HQRemote::IConnectionHandler {
		public:
			static const char *GUID_ALREADY_EXISTS_ERR_INTERNAL;
		
			typedef std::function<void(const ConnectionHandlerRakNet* handler)> MasterServerConnectedCallback;
		
			//pass <myGUID> = NULL to let internal RakNet system auto generate the GUID
			ConnectionHandlerRakNet(const RakNet::RakNetGUID* myGUID,
									const char* natPunchServerAddress,
									int natPunchServerPort,
									int preferredListenPort,
									unsigned int maxConnections,
									bool doPortForwarding,
									MasterServerConnectedCallback callback = nullptr);
			~ConnectionHandlerRakNet();
			
			RakNet::RakNetGUID getGUID() const;

			unsigned short getLanPort() const; // only valid after MasterServerConnectedCallback is invoked
			
			//IConnectionHandler implementation
			virtual bool connected() const override;
			
			MasterServerConnectedCallback serverConnectedCallback;//this is called when connection to central server finished successfully 
		private:
			struct NatPunchthroughDebugInterface_Log : public RakNet::NatPunchthroughDebugInterface
			{
				virtual void OnClientMessage(const char *msg) override;
			};
			
			using IConnectionHandler::onConnected;
			
			virtual bool startImpl() override;
			virtual void stopImpl() override;
			
			virtual HQRemote::_ssize_t sendRawDataImpl(const void* data, size_t size) override;
			virtual void flushRawDataImpl() override;
			virtual HQRemote::_ssize_t sendRawDataUnreliableImpl(const void* data, size_t size) override;
			
			void reconnectToNatServerAsync();
			
			void onConnected(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid, bool connected);
			
			RakNet::ConnectionAttemptResult connectToNatServer();
			bool tryReconnectToNatServer();
			
			void pollingMulticastData();
			void pollingProc(RakNet::RakPeerInterface* peer);
			
			static void pollingCallback(RakNet::RakPeerInterface* peer, void* userData);
			
			//port forwarding
			unsigned short getAssignedPortFromRakNetSocket() const;
			void doPortForwardingAsync();
			void removePortForwardingAsync();
			
			class AsyncData;
			AsyncData* m_portForwardAsyncData;
			
			std::atomic<uint32_t> m_natServerRemainReconnectionAttempts;
			
			HQRemote::socket_t m_multicastSocket;
			
			DataStructures::List<RakNet::RakNetSocket2* > m_raknetSockets;
			unsigned short m_portToForward;
			const bool m_doPortForwarding;
		protected:
			bool restart();
			
			virtual void stopExImpl() {}//called at the end of stopImpl()
			
			virtual void additionalUpdateProc();
			virtual void processPacket(RakNet::Packet* packet);
			virtual void onNatServerConnected(bool connected);
			virtual void onPeerConnected(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid, bool connected);
			virtual void onPeerUnreachable(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid);
		
			bool attemptNatPunchthrough(const RakNet::RakNetGUID& remoteGUID);
			bool acceptIncomingConnection();

			bool pingMulticastGroup(bool hasSendingLock);
			
			void setActiveConnection(RakNet::SystemAddress address);//set a connection as main connection for sending & receving data
			
			std::atomic<bool> m_natServerConnected;
			std::atomic<bool> m_connected;
			std::atomic<bool> m_sendingNATPunchthroughRequest;
			
			RakNet::RakPeerInterface* m_rakPeer;
			RakNet::SystemAddress m_natPunchServerAddress;
			RakNet::SystemAddress m_remotePeerAddress;
			RakNet::NatPunchthroughClient m_natPunchthroughClient;
			NatPunchthroughDebugInterface_Log m_natPunchthroughClientLogger;
			
			std::mutex m_sendingLock;
			std::mutex m_multicastLock;
			
			RakNet::BitStream m_reliableBuffer;
			RakNet::BitStream m_unreliableBuffer;
			
			unsigned int m_maxConnections;
			int m_preferredListenPort;
		};
		
		class ConnectionHandlerRakNetServer: public ConnectionHandlerRakNet {
		public:
			ConnectionHandlerRakNetServer(const RakNet::RakNetGUID* myGUID,
										  const char* natPunchServerAddress, int natPunchServerPort, MasterServerConnectedCallback callback);

			ConnectionHandlerRakNetServer(const RakNet::RakNetGUID* myGUID,
										  const char* natPunchServerAddress, int natPunchServerPort, 
										  uint64_t fixedInvitationKey,
										  MasterServerConnectedCallback callback = nullptr);
			
			uint64_t getInvitationKey() const { return m_invitationKey; }
			void createNewInvitation();//this will kick current connected player if any
		private:
			virtual void stopExImpl() override;
			
			void doRestart();
			
			virtual void onNatServerConnected(bool connected) override;
			virtual void onPeerConnected(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid, bool connected) override;
			
			virtual void additionalUpdateProc() override;
			virtual void processPacket(RakNet::Packet* packet) override;
			
			uint64_t m_reconnectionWaitStartTime;
			uint64_t m_invitationKey;

			bool m_dontWait;
			bool m_autoGenKey;//key is auto generated every time a new session is created
		};
		
		class ConnectionHandlerRakNetClient: public ConnectionHandlerRakNet {
		public:
			typedef std::function<void(const ConnectionHandlerRakNetClient* handler, bool connectOk)> ConnectivityResultCallback;
			typedef std::function<void(const ConnectionHandlerRakNetClient* handler)> OnStopCallback;

			ConnectionHandlerRakNetClient(const RakNet::RakNetGUID* myGUID,
										  const char* natPunchServerAddress,
										  int natPunchServerPort,
										  const RakNet::RakNetGUID& remoteGUID,
										  std::string&& remoteLanIp, // can be used when the remote side and local side are in the same network
										  unsigned short remoteLanPort,
										  uint64_t remoteInvitationKey,
										  bool testConnectivityOnly = false,
										  MasterServerConnectedCallback callback = nullptr);
					
			RakNet::RakNetGUID getRemoteGUID() const { return m_remoteGUID; }
			ConnectionHandlerRakNetClient& setTestingConnectivityCallback(ConnectivityResultCallback callback); // must be called before start()

			ConnectionHandlerRakNetClient& setOnStopCallback(OnStopCallback callback); // must be called before start()
		protected:
			virtual void stopExImpl() override;
			
			virtual void onNatServerConnected(bool connected) override;
			virtual void onPeerConnected(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid, bool connected) override;
			virtual void onPeerUnreachable(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid) override;
			
			virtual void processPacket(RakNet::Packet* packet) override;
			
			bool tryReconnect(RakNet::SystemAddress address);
			bool tryDiscoverLanServer(bool hasSendingLock);
			
			std::atomic<uint32_t> m_remainReconnectionAttempts;
			
			const RakNet::RakNetGUID m_remoteGUID;
			std::string m_remoteLanIpString;
			RakNet::SystemAddress m_remoteLanAddress;
			unsigned short m_remoteLanPort;
			uint64_t m_remoteInvitationKey;
			bool m_tryConnectToLanIp;
			const bool m_testOnly;
			ConnectivityResultCallback m_testCallback;
			OnStopCallback m_onStopCallback;
		};
		
		class RakNetValidGuidChecker: public ConnectionHandlerRakNet {
		public:
			static const size_t MAX_GUIDS_TO_CHECK = 5;
		
			RakNetValidGuidChecker(const RakNet::RakNetGUID* myGUID,
								   const char* natPunchServerAddress,
								   int natPunchServerPort);
								   
			//<numGuidsToCheck> must not larger than MAX_GUIDS_TO_CHECK
			void checkIfGuidsValid(const RakNet::RakNetGUID* listGuid, size_t numGuidsToCheck);
			
			std::function<void(RakNetValidGuidChecker* checker, const RakNet::RakNetGUID* listGuid, size_t numGuidsToCheck)> invalidGuidsNotificationCallback;
		protected:
			virtual void onNatServerConnected(bool connected) override;
			virtual void processPacket(RakNet::Packet* packet) override;
			
			std::mutex m_pendingDataLock;
			std::list<std::shared_ptr<RakNet::BitStream> > m_pendingDataToSend;
		};
	}
}
#endif /* ConnectionHandlerRakNet_hpp */
