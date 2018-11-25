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

#include <RakNetSocket2.h>
#include <RakPeer.h>

#include "ConnectionHandlerRakNet.hpp"

#include "../../third-party/miniupnp/miniupnpc/miniupnpc.h"
#include "../../third-party/miniupnp/miniupnpc/upnpcommands.h"
#include "../../third-party/miniupnp/miniupnpc/upnperrors.h"

#include <stdlib.h>
#include <assert.h>
#include <sstream>

#ifdef WIN32
#	include <windows.h>

#	define _INADDR_ANY INADDR_ANY
#	define MSGSIZE_ERROR WSAEMSGSIZE
#	define CONN_INPROGRESS WSAEWOULDBLOCK

typedef int socklen_t;

#else//#ifdef WIN32

#	define SOCKET_ERROR -1
#	define INVALID_SOCKET -1
#	define SD_BOTH SHUT_RDWR
#	define closesocket close
#	define _INADDR_ANY htonl(INADDR_ANY)
#	define MSGSIZE_ERROR EMSGSIZE
#	define CONN_INPROGRESS EINPROGRESS

#endif//#ifdef WIN32

#ifndef max
#	define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#	define min(a,b) ((a) < (b) ? (a) : (b))
#endif

//#define DEBUG

// this flag is not really needed, if the same guid is used to connect to nat server,
// it will return ID_ALREADY_CONNECTED
#define USE_NAT_SERVER_ACCEPT_SYSTEM 0

#define SKIP_LAN_CONNECTION 0

namespace Nes {
	namespace Remote {
		static const uint64_t THIS_GAME_ID = 0;

		static const uint32_t DEFAULT_RELAY_ATTEMPT_TIMEOUT_MS = 7000;
		static const unsigned short DEFAULT_SERVER_PORT = 62989;
		static const unsigned short DEFAULT_MAX_INCOMING_CONNECTIONS = 100;
		
		static const char MULTICAST_ADDRESS[] = "226.1.1.1";
		static const unsigned short MULTICAST_PORT = 60289;
		static const char MULTICAST_MAGIC_STRING[] = "864bb9cf-f8e5-44cf-bb0d-95b82683f200";
		
		static const size_t RELIABLE_MSG_MAX_SIZE = 1400;
		static const size_t UNRELIABLE_MSG_MAX_SIZE = 1024;
		
		static_assert(ID_USER_PACKET_ENUM <= (0xFF - 1), "Unexpected max packet id" );
		
		static const uint32_t MAX_RECONNECTION_ATTEMPTS = 3;
		
		static const float MAX_RECONNECTION_WAIT_DURATION = 20.f;
		
		enum USER_PACKET_ID: uint8_t {
			ID_USER_PACKET_ENUM_RELIABLE = ID_USER_PACKET_ENUM + 1,
			ID_USER_PACKET_ENUM_ACCEPTED,
			ID_USER_PACKET_ENUM_REFUSED,
			ID_USER_PACKET_ENUM_TEST_CONNECTIVITY,
			ID_USER_PACKET_ENUM_REQUEST_TO_JOIN,
			ID_USER_PACKET_ENUM_REQUEST_TO_REJOIN,
			
			ID_USER_PACKET_RECONNECT_NAT_SERVER,
			ID_USER_PACKET_REINVITE_FRIEND,
			
			//messages for NAT server 
			ID_USER_PACKET_ENUM_CHECK_GUID = ID_USER_PACKET_ENUM + 100,
			ID_USER_PACKET_ENUM_INVALID_GUID_LIST,
#if USE_NAT_SERVER_ACCEPT_SYSTEM
			ID_USER_PACKET_NAT_SERVER_ERR_GUID_ALREADY_EXIST,
			ID_USER_PACKET_NAT_SERVER_ACCEPTED,
#endif
			ID_USER_PACKET_CREATE_PUBLIC_ROOM = ID_USER_PACKET_ENUM_INVALID_GUID_LIST + 3,
		};

		static std::mutex g_globalLock;
		static std::mutex g_asyncLock;
		
		void ConnectionHandlerRakNet::NatPunchthroughDebugInterface_Log::OnClientMessage(const char *msg) {
			HQRemote::Log("%s\n", msg);
		}
		
#if defined __ANDROID__	|| defined WIN32
		static inline uint64_t ntohll(uint64_t src) {
			if (!RakNet::BitStream::IsNetworkOrder())
			{	
				uint64_t re;
				RakNet::BitStream::ReverseBytes((unsigned char*)&src, (unsigned char*)&re, sizeof(src));
				return re;
			}
			else
				return src;
		}
		
		static inline uint64_t htonll(uint64_t src) {
			return ntohll(src);
		}
#endif

		static std::ostream& operator << (std::ostream& os, const RakNet::SystemAddress& address) {
			char addressStr[256];
			address.ToString(true, addressStr);

			return os << addressStr;
		}

		static std::ostream& operator << (std::ostream& os, const RakNet::RakNetGUID& guid) {
			char guid_str[128];
			guid.ToString(guid_str);

			return os << guid_str;
		}

		struct LogStream {
			~LogStream() {
				HQRemote::Log("%s", m_ss.str().c_str());
			}

			template <class T>
			LogStream& operator << (const T& o) {
				m_ss << o;
				return *this;
			}

			template <class T>
			LogStream& operator << (const T* o) {
				m_ss << o;
				return *this;
			}
		private:
			std::ostringstream m_ss;
		};

		/*----- MyRakPeer -----*/
		class MyRakPeer: public RakNet::RakPeer {
		public:
			MyRakPeer(const RakNet::RakNetGUID* _myGUID)
			{
				if (_myGUID != NULL)
				{
					this->myGuid = *_myGUID;
				}

				char guid_str[128];
				this->myGuid.ToString(guid_str);

				HQRemote::Log("RakPeer created with guid: %s\n", guid_str);
			}
		};

		/*---------   ConnectionHandlerRakNet -------*/
		const char *ConnectionHandlerRakNet::GUID_ALREADY_EXISTS_ERR_INTERNAL = "guid_exist";

		class ConnectionHandlerRakNet::AsyncData{
		public:
			ConnectionHandlerRakNet* ref;
		};

		ConnectionHandlerRakNet::ConnectionHandlerRakNet(const RakNet::RakNetGUID* myGUID,
														 const char* natPunchServerAddressStr, int natPunchServerPort,
														 const char* proxyServerAddress, int proxyServerPort,
														 int preferredListenPort,
														 unsigned int maxConnections,
														 bool doPortForwarding,
														 MasterServerConnectedCallback callback)
		:m_connected(false), m_natServerConnected(false), m_sendingNATPunchthroughRequest(false),
		m_natServerRemainReconnectionAttempts(MAX_RECONNECTION_ATTEMPTS),
		m_natPunchServerAddress(natPunchServerAddressStr, natPunchServerPort),
		m_reliableBuffer(RELIABLE_MSG_MAX_SIZE + 1),
		m_unreliableBuffer(UNRELIABLE_MSG_MAX_SIZE + 1),
		m_multicastSocket(INVALID_SOCKET),
		m_preferredListenPort(preferredListenPort),
		m_maxConnections(maxConnections),
		serverConnectedCallback(callback),
		m_portForwardAsyncData(nullptr),
		m_doPortForwarding(doPortForwarding),
		m_portToForward(0)
		{
			std::lock_guard<std::mutex> lg(g_globalLock);

			m_rakPeer = new MyRakPeer(myGUID);
			m_rakPeer->AttachPlugin(&m_natPunchthroughClient);

			auto natConfig = m_natPunchthroughClient.GetPunchthroughConfiguration();
			natConfig->MAX_PREDICTIVE_PORT_RANGE = 2;
#if defined DEBUG || defined _DEBUG
			m_natPunchthroughClient.SetDebugInterface(&m_natPunchthroughClientLogger);
#endif

			if (proxyServerAddress && proxyServerPort) {
				m_proxyCoordinatorAddress = RakNet::SystemAddress(proxyServerAddress,
																  proxyServerPort);

				m_proxyClient.SetResultHandler(this);
				m_rakPeer->AttachPlugin(&m_proxyClient);
			}
		}

		ConnectionHandlerRakNet::~ConnectionHandlerRakNet()
		{
			std::lock_guard<std::mutex> lg(g_globalLock);

			delete m_rakPeer;
		}

		uint64_t ConnectionHandlerRakNet::getIdForThisApp() {
			return THIS_GAME_ID;
		}

		RakNet::RakNetGUID ConnectionHandlerRakNet::getGUID() const {
			return m_rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS);
		}

		unsigned short ConnectionHandlerRakNet::getLanPort() const {
			return getAssignedPortFromRakNetSocket();
		}

		//IConnectionHandler implementation
		bool ConnectionHandlerRakNet::connected() const
		{
			return m_connected.load(std::memory_order_relaxed);
		}

		bool ConnectionHandlerRakNet::isLimitedBySendingBandwidth() const {
			if (!connected())
				return false;
			m_rakPeer->GetStatistics(m_remotePeerAddress, &m_stats);
			return m_stats.isLimitedByCongestionControl;
		}

		bool ConnectionHandlerRakNet::startImpl()
		{
			//create multicast socket
			{
				m_multicastSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

				int re;

				int yes = 1;

				//bind address
				struct sockaddr_in addr = {0};
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = _INADDR_ANY;
				addr.sin_port = htons(MULTICAST_PORT);

				re = setsockopt(m_multicastSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes,sizeof(yes));
				HQRemote::Log("* Multicast setsockopt(SO_REUSEADDR) returned %d\n", re);


				re = bind(m_multicastSocket, (struct sockaddr *)&addr, sizeof(addr));
				HQRemote::Log("* Multicast bind() returned %d\n", re);

				//join multicast group with all available network interfaces
				struct ip_mreq mreq;
				mreq.imr_multiaddr.s_addr=inet_addr(MULTICAST_ADDRESS);

				std::vector<struct in_addr> interface_addresses;
				HQRemote::SocketServerHandler::platformGetLocalAddressesForMulticast(interface_addresses);

				for (auto &_interface : interface_addresses) {
					mreq.imr_interface = _interface;

					re = setsockopt(m_multicastSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));

					char addr_buffer[20];
					if (HQRemote::SocketConnectionHandler::platformIpv4AddrToString(&_interface, addr_buffer, sizeof(addr_buffer)) != NULL)
						HQRemote::Log("Multicast setsockopt(IP_ADD_MEMBERSHIP, %s) returned %d\n", addr_buffer, re);
					else
						HQRemote::Log("Multicast setsockopt(IP_ADD_MEMBERSHIP) returned %d\n", re);
				}

				//disable loopback
				char loopback = 0;
				re = setsockopt(m_multicastSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loopback, sizeof(loopback));
				HQRemote::Log("* Multicast setsockopt(IP_MULTICAST_LOOP) returned %d\n", re);

				//set max TTL to reach more than one subnets
				unsigned char ttl = 3;
				re = setsockopt(m_multicastSocket, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl));
				HQRemote::Log("* Multicast setsockopt(IP_MULTICAST_TTL) returned %d\n", re);
			}

			/*---- initialize RakNet ----*/
			m_reliableBuffer.Reset();
			m_unreliableBuffer.Reset();

			m_rakPeer->SetUserUpdateThread(pollingCallback, this);

			//Use 2 socket descriptors;
			//one will make the OS assign us a random port
			//one will use preferred port
			RakNet::SocketDescriptor socketDescriptor[2];
			socketDescriptor[0].port = 0;
			socketDescriptor[1].port = (unsigned short)m_preferredListenPort;
			unsigned int numSocketDesc = socketDescriptor[0].port == socketDescriptor[1].port ? 1 : 2;

			//Allow 3 types of connections: one for the other peer, one for the NAT server, one for proxy server.
			int numFixedConnections = 1;
			if (m_proxyCoordinatorAddress != RakNet::UNASSIGNED_SYSTEM_ADDRESS)
				numFixedConnections ++;
			auto startRe = m_rakPeer->Startup(numFixedConnections + m_maxConnections, socketDescriptor, numSocketDesc);
			if (startRe == RakNet::SOCKET_PORT_ALREADY_IN_USE || startRe == RakNet::SOCKET_FAILED_TO_BIND)
			{
				//retry with system assigned port
				socketDescriptor[1].port = 0;

				startRe = m_rakPeer->Startup(numFixedConnections+ m_maxConnections, socketDescriptor, 1);
			}

			if (startRe != RakNet::RAKNET_STARTED && startRe != RakNet::RAKNET_ALREADY_STARTED)
				return false;

			m_portToForward = 0;//will get this one after we are connected to NAT server
			m_rakPeer->GetSockets(m_raknetSockets);

			return restart();
		}

		void ConnectionHandlerRakNet::stopImpl()
		{
			{
				std::lock_guard<std::mutex> lg(g_globalLock);
				if (m_portForwardAsyncData != nullptr){
					m_portForwardAsyncData->ref = nullptr; //prevent port fowarding async operation from invoking a callback on this instance
					m_portForwardAsyncData = nullptr;
				}
			}

			removePortForwardingAsync();//remove port forwarding entry

			//release RakNet
			m_rakPeer->ReleaseSockets(m_raknetSockets);

			m_rakPeer->SetUserUpdateThread(NULL, NULL);
			m_rakPeer->Shutdown(1000);

			{
				std::lock_guard<std::mutex> lg(m_multicastLock);
				if (m_multicastSocket != INVALID_SOCKET)
					closesocket(m_multicastSocket);
				m_multicastSocket = INVALID_SOCKET;
			}

			m_portToForward = 0;

			{
				std::lock_guard<std::mutex> lg(m_sendingLock);

				m_sendingNATPunchthroughRequest = false;
				m_remotePeerAddress = RakNet::SystemAddress();
				m_connected = false;

				stopExImpl();
			}
		}

		void ConnectionHandlerRakNet::reconnectToNatServerAsync()
		{
			unsigned char restartId = ID_USER_PACKET_RECONNECT_NAT_SERVER;
			m_rakPeer->SendLoopback((char*)&restartId, sizeof(restartId));
		}

		unsigned short ConnectionHandlerRakNet::getAssignedPortFromRakNetSocket() const {
			unsigned int socketIdx;
			unsigned short assignedPort;
			for (socketIdx = 0; socketIdx < m_raknetSockets.Size(); socketIdx++)
			{
				if (m_raknetSockets[socketIdx]->GetUserConnectionSocketIndex()==0)
				{
					assignedPort = m_raknetSockets[socketIdx]->GetBoundAddress().GetPort();
					break;
				}
			}

			return assignedPort;
		}

		static void removePortForwardingAsyncImpl(unsigned short port)
		{
			std::thread asyncThread([port]{
				std::unique_lock<std::mutex> alg(g_asyncLock);//only one async operation can be executed at a time

				int timeout = 2000;

				HQRemote::Log("* Removing forwarded port = %d\n", (int)port);

				// Behind a NAT. Try to open with UPNP to avoid doing NAT punchthrough
				struct UPNPDev * devlist = 0;
				devlist = upnpDiscover(timeout, 0, 0, 0, 0, 2, 0);

				if (devlist)
				{
					char lanaddr[64];	/* my ip address on the LAN */
					struct UPNPUrls urls;
					struct IGDdatas data;
					if (UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr))==1)
					{
						char iport[32];
						sprintf(iport, "%d", (int)port);
						char eport[32];
						strcpy(eport, iport);

						HQRemote::Log("* Removing forwarded port = %d using %s\n", (int)port, urls.controlURL);

						// Version miniupnpc-1.6.20120410
						int r = UPNP_DeletePortMapping(urls.controlURL,
													   data.first.servicetype,
													   eport, "UDP", 0);

						if(r!=UPNPCOMMAND_SUCCESS)
							HQRemote::Log("* UPNP_DeletePortMapping(%s) failed with code %d (%s)\n",
										  eport, r, strupnperror(r));
						else
							HQRemote::Log("* UPNP_DeletePortMapping(%s) succeeded\n", eport);
					}//if (UPNP_GetValidIGD)
				}//if (devlist)

				HQRemote::Log("* Finished removing forwarded port = %d\n", (int)port);
			});

			asyncThread.detach();
		}

		void ConnectionHandlerRakNet::removePortForwardingAsync() {
			auto port = m_portToForward;
			if (port == 0)
				return;

			removePortForwardingAsyncImpl(port);
		}

		void ConnectionHandlerRakNet::doPortForwardingAsync() {
			std::lock_guard<std::mutex> lg(g_globalLock);

			auto port = m_portToForward;
			if (port == 0)
				return;

			auto asyncData = m_portForwardAsyncData = new AsyncData();
			m_portForwardAsyncData->ref = this;

			std::thread asyncThread([asyncData, port]{
				std::unique_lock<std::mutex> alg(g_asyncLock);//only one async operation can be executed at a time

				HQRemote::Log("* Forwarding port = %d\n", (int)port);

				int timeout = 2000;

				// Behind a NAT. Try to open with UPNP to avoid doing NAT punchthrough
				struct UPNPDev * devlist = 0;
				devlist = upnpDiscover(timeout, 0, 0, 0, 0, 2, 0);

				bool succeeded = true;

				if (devlist)
				{
					char lanaddr[64];	/* my ip address on the LAN */
					struct UPNPUrls urls;
					struct IGDdatas data;
					if (UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr))==1)
					{
						char iport[32];
						sprintf(iport, "%d", (int)port);
						char eport[32];
						strcpy(eport, iport);

						HQRemote::Log("* Forwarding port = %d using %s\n", (int)port, urls.controlURL);

						// Version miniupnpc-1.6.20120410
						int r = UPNP_AddPortMapping(urls.controlURL, data.first.servicetype,
													eport, iport, lanaddr, 0, "UDP", 0, "0");

						if(r!=UPNPCOMMAND_SUCCESS)
						{
							HQRemote::Log("AddPortMapping(%s, %s, %s) failed with code %d (%s)\n",
										  eport, iport, lanaddr, r, strupnperror(r));

							succeeded = false;
						}

						char intPort[6];
						char intClient[16];

						// Version miniupnpc-1.6.20120410
						char desc[128];
						char enabled[128];
						char leaseDuration[128];
						r = UPNP_GetSpecificPortMappingEntry(urls.controlURL,
															 data.first.servicetype,
															 eport, "UDP", 0,
															 intClient, intPort,
															 desc, enabled, leaseDuration);

						if(r!=UPNPCOMMAND_SUCCESS)
						{
							HQRemote::Log("GetSpecificPortMappingEntry() failed with code %d (%s)\n",
									r, strupnperror(r));
						}
					}//if (UPNP_GetValidIGD)
				}//if (devlist)

				HQRemote::Log("* Finished forwarding port = %d success=%d\n", (int)port, succeeded ? 1 : 0);

				//finished
				std::lock_guard<std::mutex> lg(g_globalLock);

				auto handler = asyncData->ref;

				if (handler)
				{
					handler->m_portForwardAsyncData = nullptr;//invalidate the data referece of this thread

					//reconnect to the NAT server with new port mapping
					handler->reconnectToNatServerAsync();
				}
				else
					removePortForwardingAsyncImpl(port);

				delete asyncData;
			});

			asyncThread.detach();
		}

		bool ConnectionHandlerRakNet::attemptNatPunchthrough(const RakNet::RakNetGUID& remoteGUID) {
			char guid_str[128];
			remoteGUID.ToString(guid_str);
			HQRemote::Log("* Attempt to openNAT to guid=%s", guid_str);

			m_sendingNATPunchthroughRequest = true;
			auto re = m_natPunchthroughClient.OpenNAT(remoteGUID, m_natPunchServerAddress);
			return re;
		}

		bool ConnectionHandlerRakNet::acceptIncomingConnection() {
			m_rakPeer->SetMaximumIncomingConnections(m_maxConnections);
			return true;
		}

		bool ConnectionHandlerRakNet::pingMulticastGroup(bool hasSendingLock) {
			std::unique_lock<std::mutex> lg(m_sendingLock, std::defer_lock);
			std::lock_guard<std::mutex> lg2(m_multicastLock);

			// if caller hasn't locked the 'sendingLock' yet then lock it here
			if (!hasSendingLock)
				lg.lock();

			//multicast message = | type | magic string |  my guid |
			unsigned char ping_msg[sizeof (MULTICAST_MAGIC_STRING) + sizeof(uint64_t)];

			auto my_guid = m_rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS).g;
			uint64_t n_guid = htonll(my_guid);

			unsigned char type = ID_UNCONNECTED_PING;
			memcpy(ping_msg, &type, sizeof(type));
			memcpy(ping_msg + 1, MULTICAST_MAGIC_STRING, sizeof(MULTICAST_MAGIC_STRING) - 1);
			memcpy(ping_msg + sizeof(MULTICAST_MAGIC_STRING), &n_guid, sizeof(n_guid));


			{
				//use connected socket to NAT server to send raw data
				RakNet::RNS2_SendParameters sp;
				sp.data = (char*)ping_msg;
				sp.length = sizeof(ping_msg);
				sp.systemAddress.FromStringExplicitPort(MULTICAST_ADDRESS, MULTICAST_PORT);
				sp.ttl = 4;

				for (unsigned int i=0; i < m_raknetSockets.Size(); i++)
				{
					if (m_raknetSockets[i]->GetUserConnectionSocketIndex()==0)
					{
						m_raknetSockets[i]->Send(&sp, __FILE__, __LINE__);
						break;
					}
				}
			}
			return true;
		}

		void ConnectionHandlerRakNet::pollingMulticastData() {
			std::lock_guard<std::mutex> lg(m_multicastLock);

			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;

			fd_set sset;

			FD_ZERO(&sset);
			FD_SET(m_multicastSocket, &sset);

			if (select(m_multicastSocket + 1, &sset, NULL, NULL, &timeout) == 1 && FD_ISSET(m_multicastSocket, &sset))
			{
				unsigned char msg[MAXIMUM_MTU_SIZE];

				socklen_t from_addr_len = sizeof(sockaddr_in);
				sockaddr_in from_addr;

				int re = (int)recvfrom(m_multicastSocket, (char*)msg,  sizeof(msg), 0, (sockaddr*) &from_addr, &from_addr_len);
				if (re > 0)
				{
					//parse source address
					auto src_port = ntohs(from_addr.sin_port);

					RakNet::SystemAddress rakAddress;

					rakAddress.systemIndex=(RakNet::SystemIndex)-1;
					rakAddress.address.addr4 = from_addr;

#if defined DEBUG || defined _DEBUG
					rakAddress.debugPort = src_port;
#endif

					char src_addr_str[128];
					rakAddress.ToString(false, src_addr_str);

					switch (msg[0])
					{
						case ID_UNCONNECTED_PING:
							//multicast message = ID_UNCONNECTED_PING | magic string |  my guid |
							if (re >= sizeof (MULTICAST_MAGIC_STRING) + sizeof(uint64_t) &&
								memcmp(msg + 1, MULTICAST_MAGIC_STRING, sizeof(MULTICAST_MAGIC_STRING) - 1) == 0)
							{
								//this is multi cast ping
								RakNet::RakNetGUID srcGuid;
								memcpy(&srcGuid.g, msg + sizeof(MULTICAST_MAGIC_STRING), sizeof(srcGuid.g));
								srcGuid.g = ntohll(srcGuid.g);

								//avertise our system back to source system
								auto guid = m_rakPeer->GetGuidFromSystemAddress(RakNet::UNASSIGNED_SYSTEM_ADDRESS);
								if (srcGuid != guid)
								{
#if 1 || defined DEBUG || defined _DEBUG
									HQRemote::Log("* Got multicast ping from %s:%d\n", src_addr_str, (int)src_port);
#endif
									auto n_guid = htonll(guid.g);
									m_rakPeer->AdvertiseSystem(src_addr_str, src_port, (const char*)&n_guid, sizeof n_guid);
								}
							}//multicast ping
							break;
					}//switch (msg[0])

				}//if (re > 0)
			}//if (select(...))
		}

		HQRemote::_ssize_t ConnectionHandlerRakNet::sendRawDataImpl(const void* data, size_t size)
		{
			std::lock_guard<std::mutex> lg(m_sendingLock);

			if (m_connected.load(std::memory_order_relaxed))
			{
				if (m_reliableBuffer.GetNumberOfBytesUsed() == 0)
				{
					//write message's tag
					m_reliableBuffer.Write(static_cast<unsigned char>(ID_USER_PACKET_ENUM_RELIABLE));
				}

				auto remainBufferSize = RELIABLE_MSG_MAX_SIZE - m_reliableBuffer.GetNumberOfBytesUsed();

				auto copySize = min(size, remainBufferSize);

				if (copySize == 0)
				{
					flushRawDataNoLock();
					copySize = min(size, RELIABLE_MSG_MAX_SIZE);
				}

				m_reliableBuffer.Write((const char*)data, (unsigned int)copySize);

				return copySize;
			}//m_connected;

			return -1;
		}
		void ConnectionHandlerRakNet::flushRawDataImpl() {
			std::lock_guard<std::mutex> lg(m_sendingLock);

			flushRawDataNoLock();
		}

		void ConnectionHandlerRakNet::flushRawDataNoLock()
		{
			if (m_connected.load(std::memory_order_relaxed) &&
				m_reliableBuffer.GetNumberOfBytesUsed() > 0)
			{
				m_rakPeer->Send(&m_reliableBuffer, HIGH_PRIORITY, RELIABLE_ORDERED, 1, m_remotePeerAddress, false);

				m_reliableBuffer.Reset();

				//write message's tag
				m_reliableBuffer.Write(static_cast<unsigned char>(ID_USER_PACKET_ENUM_RELIABLE));
			}
		}

		HQRemote::_ssize_t ConnectionHandlerRakNet::sendRawDataUnreliableImpl(const void* data, size_t size)
		{
			std::lock_guard<std::mutex> lg(m_sendingLock);

			if (m_connected.load(std::memory_order_relaxed))
			{
				m_unreliableBuffer.Reset();
				//write message's tag
				m_unreliableBuffer.Write(static_cast<unsigned char>(ID_USER_PACKET_ENUM));

				size = min(size, UNRELIABLE_MSG_MAX_SIZE);

				m_unreliableBuffer.Write((const char*)data, (unsigned int) size);

				m_rakPeer->Send(&m_unreliableBuffer, HIGH_PRIORITY, UNRELIABLE, 1, m_remotePeerAddress, false);

				return size;
			}//if (m_connected.load(std::memory_order_relaxed))

			return -1;
		}

		void ConnectionHandlerRakNet::onConnected(RakNet::SystemAddress address, RakNet::RakNetGUID guid, bool connected) {
			std::lock_guard<std::mutex> lg(m_sendingLock);

			if (connected)
			{
				if(address ==
				   m_natPunchServerAddress)
				{
					HQRemote::Log("* Successfully connected to the NAT punchthrough server.\n");

					m_natServerRemainReconnectionAttempts = MAX_RECONNECTION_ATTEMPTS;

					if (m_portToForward == 0 && m_doPortForwarding)
					{
						//firt time setup
						m_portToForward = getAssignedPortFromRakNetSocket();

						m_rakPeer->CloseConnection(address, true, 1, HIGH_PRIORITY);//close the connection to NAT server, we will connect to it again after port forwarding finishes

						doPortForwardingAsync();
					}
					else
					{
						if (!m_natServerConnected)
						{
							onNatServerConnected(true);

							m_natServerConnected = true;

							//invoke callback
							if (this->serverConnectedCallback)
								this->serverConnectedCallback(this);
						}//if (!m_natServerConnected)
					}
				}
				else
				{
					char addressStr[256];
					address.ToString(true, addressStr);
					HQRemote::Log("* Connection to the remote peer %s successfully established.\n", addressStr);

					onPeerConnected(address, guid, connected);
				}
			}//if (connected)
			else {
				if(address ==
				   m_natPunchServerAddress)
				{
					HQRemote::Log("* Disconnected from the NAT punchthrough server.\n");

					bool portForwardingInProgress = false;
					{
						std::lock_guard<std::mutex> lg(g_globalLock);
						portForwardingInProgress = m_portForwardAsyncData != nullptr;
					}

					if (!portForwardingInProgress)//ignore if port forwarding is still in progress
					{
						if (!m_connected)
						{
							if (!tryReconnectToNatServer())
							{
								if (!m_natServerConnected)
									onNatServerConnected(false);

								m_natServerConnected = false;

								setInternalError("Service is unavailable at the moment. Please try again later");//TODO: localize
							}
						}
						else
						{
							if (!m_natServerConnected)
								onNatServerConnected(false);

							m_natServerConnected = false;
						}
					}//if (!portForwardingInProgress)
				}
				else
				{
					char addressStr[256];
					address.ToString(true, addressStr);
					HQRemote::Log("* Disconnected from the remote peer %s.\n", addressStr);

					onPeerConnected(address, guid, connected);
				}
			}//if (connected)
		}

		bool ConnectionHandlerRakNet::restart()
		{
			if (!m_natServerConnected)
			{
				m_natServerRemainReconnectionAttempts = MAX_RECONNECTION_ATTEMPTS;

				// connect to NAT server
				auto connectRe = connectToNatServer();

				if (connectRe != RakNet::CONNECTION_ATTEMPT_STARTED &&
					connectRe != RakNet::CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS &&
					connectRe != RakNet::ALREADY_CONNECTED_TO_ENDPOINT)
					return false;
			}
			else
			{
				m_natServerConnected = false;
				onConnected(m_natPunchServerAddress, RakNet::RakNetGUID(), true);
			}

			// connecto to proxy server
			tryConnectToProxyServer();

			return true;
		}

		RakNet::ConnectionAttemptResult ConnectionHandlerRakNet::connectToNatServer() {
			//connect to NAT punchthrough server
			char natServerAddressStr[256];
			m_natPunchServerAddress.ToString(false, natServerAddressStr);

			return m_rakPeer->Connect(natServerAddressStr, m_natPunchServerAddress.GetPort(), NULL, 0);
		}

		bool ConnectionHandlerRakNet::tryReconnectToNatServer() {
			if (m_natServerRemainReconnectionAttempts == 0)
				return false;

			auto connectRe = connectToNatServer();

			if (connectRe != RakNet::CONNECTION_ATTEMPT_STARTED &&
				connectRe != RakNet::CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS &&
				connectRe != RakNet::ALREADY_CONNECTED_TO_ENDPOINT)
				return false;

			if (connectRe == RakNet::CONNECTION_ATTEMPT_STARTED)
				m_natServerRemainReconnectionAttempts --;

			return true;
		}

		RakNet::ConnectionAttemptResult ConnectionHandlerRakNet::tryConnectToProxyServer() {
			if (m_proxyCoordinatorAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS)
				return RakNet::CANNOT_RESOLVE_DOMAIN_NAME;
			char addressStr[256];
			m_proxyCoordinatorAddress.ToString(false, addressStr);

			return m_rakPeer->Connect(addressStr, m_proxyCoordinatorAddress.GetPort(), NULL, 0);
		}

		void ConnectionHandlerRakNet::setActiveConnection(RakNet::SystemAddress address) {
			std::lock_guard<std::mutex> lg(m_sendingLock);

			bool isReconnection = m_connected && m_remotePeerAddress == address;

			m_connected = true;
			onConnected(isReconnection);//tell base class IConnectionHandler

			m_remotePeerAddress = address;

			//disconnect from NAT punchthrough server
			m_sendingNATPunchthroughRequest = false;
			m_rakPeer->CloseConnection(m_natPunchServerAddress, true, 1, HIGH_PRIORITY);

			// disconnect from proxy server
			if (m_proxyCoordinatorAddress != RakNet::UNASSIGNED_SYSTEM_ADDRESS)
				m_rakPeer->CloseConnection(m_proxyCoordinatorAddress, true, 1, HIGH_PRIORITY);
		}

		void ConnectionHandlerRakNet::processPacket(RakNet::Packet* packet) {
			//default implementation
		}

		void ConnectionHandlerRakNet::onNatServerConnected(bool connected) {
			//default implementation
		}

		void ConnectionHandlerRakNet::onPeerConnected(RakNet::SystemAddress address, RakNet::RakNetGUID guid, bool connected) {
			//default implementation
			if (!connected) {
				if(address == m_remotePeerAddress)
				{
					m_connected = false;

					// IConnectionHandler::onDisconnected();
					onDisconnected(); // notify base class
				}
			}
		}

		void ConnectionHandlerRakNet::onPeerUnreachable(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid) {
			//default implementation
		}

		void ConnectionHandlerRakNet::additionalUpdateProc() {
			//default implementation
		}

		void ConnectionHandlerRakNet::pollingProc(RakNet::RakPeerInterface* peer)
		{
			assert(peer == m_rakPeer);

			for (auto packet = m_rakPeer->Receive();
				 packet;
				 m_rakPeer->DeallocatePacket(packet),
				 packet = m_rakPeer->Receive())
			{
				//check the packet identifier
				switch(packet->data[0])
				{
					case ID_USER_PACKET_RECONNECT_NAT_SERVER:
					{
						m_natServerRemainReconnectionAttempts = MAX_RECONNECTION_ATTEMPTS;
						//close connection to the NAT server, the reconnection with new port mapping will be attempted later
						m_rakPeer->CloseConnection(m_natPunchServerAddress, true, 1, HIGH_PRIORITY);
						auto re = connectToNatServer();
					}
						break;
					case ID_CONNECTION_REQUEST_ACCEPTED:
						//if remote address is not NAT server, this should only be triggered on client side
#if USE_NAT_SERVER_ACCEPT_SYSTEM
						if (packet->systemAddress != m_natPunchServerAddress)//for NAT server we handle it later in ID_USER_PACKET_NAT_SERVER_ACCEPTED
#endif//USE_NAT_SERVER_ACCEPT_SYSTEM
							onConnected(packet->systemAddress, packet->guid, true);
						break;
					case ID_ALREADY_CONNECTED:
#if 1
						if (packet->systemAddress != m_natPunchServerAddress)
#endif
							break;
#if USE_NAT_SERVER_ACCEPT_SYSTEM
					case ID_USER_PACKET_NAT_SERVER_ERR_GUID_ALREADY_EXIST:
#endif
						setInternalError(GUID_ALREADY_EXISTS_ERR_INTERNAL);//TODO: this is the agreement between this and user of this code

						m_natServerRemainReconnectionAttempts = 0;//no more reconnection attempt
						m_rakPeer->CloseConnection(m_natPunchServerAddress, true, 1, HIGH_PRIORITY);

						if (m_proxyCoordinatorAddress != RakNet::UNASSIGNED_SYSTEM_ADDRESS)
							m_rakPeer->CloseConnection(m_proxyCoordinatorAddress, true, 1, HIGH_PRIORITY);

						HQRemote::Log("* We already connected to NAT server elsewhere\n");
						break;
#if USE_NAT_SERVER_ACCEPT_SYSTEM
					case ID_USER_PACKET_NAT_SERVER_ACCEPTED: //NAT server accepted
						onConnected(packet->systemAddress, packet->guid, true);
						break;
#endif//USE_NAT_SERVER_ACCEPT_SYSTEM
					case ID_NEW_INCOMING_CONNECTION:
					{
						//this should triggered on server side
						onConnected(packet->systemAddress, packet->guid, true);
					}
						break;
					case ID_NAT_PUNCHTHROUGH_SUCCEEDED:
						HQRemote::Log("* NAT punchthrough succeeded!\n");

						if(m_sendingNATPunchthroughRequest)
						{
							m_sendingNATPunchthroughRequest = false;

							char addressStr[256];
							packet->systemAddress.ToString(false, addressStr);

							HQRemote::Log("* Connecting to the peer %s ...\n", addressStr);
							m_rakPeer->Connect(addressStr,
											   packet->systemAddress.GetPort(), 0, 0);
						}
						break;
						/// NATPunchthrough plugin: Destination system is not connected to the server. Bytes starting at offset 1 contains the
						///  RakNetGUID destination field of NatPunchthroughClient::OpenNAT().
					case ID_NAT_TARGET_NOT_CONNECTED:
						/// NATPunchthrough plugin: Destination system is not responding to ID_NAT_GET_MOST_RECENT_PORT. Possibly the plugin is not installed.
						///  Bytes starting at offset 1 contains the RakNetGUID  destination field of NatPunchthroughClient::OpenNAT().
					case ID_NAT_TARGET_UNRESPONSIVE:
						/// NATPunchthrough plugin: The server lost the connection to the destination system while setting up punchthrough.
						///  Possibly the plugin is not installed. Bytes starting at offset 1 contains the RakNetGUID  destination
						///  field of NatPunchthroughClient::OpenNAT().
					case ID_NAT_CONNECTION_TO_TARGET_LOST:
					{
						HQRemote::Log("* NAT punchthrough error id=%d\n", (int)packet->data[0]);

						RakNet::RakNetGUID guid;

						memcpy(&guid.g, packet->data + 1, sizeof(guid.g));

						guid.g = ntohll(guid.g);
						onPeerUnreachable(RakNet::SystemAddress(), guid);
					}
						break;
						/// NATPunchthrough plugin: This punchthrough is already in progress. Possibly the plugin is not installed.
						///  Bytes starting at offset 1 contains the RakNetGUID destination field of NatPunchthroughClient::OpenNAT().
					case ID_NAT_ALREADY_IN_PROGRESS:
					{
						//TODO:
					}
						break;

					case ID_USER_PACKET_ENUM:
					{
						onReceivedUnreliableDataFragment(packet->data + 1, packet->length - 1);
					}
						break;
					case ID_USER_PACKET_ENUM_RELIABLE:
					{
						onReceiveReliableData(packet->data + 1, packet->length - 1);
					}
						break;
					case ID_REMOTE_DISCONNECTION_NOTIFICATION:
						HQRemote::Log("* Remote peer has disconnected.\n");
						onConnected(packet->systemAddress, packet->guid, false);
						break;
					case ID_REMOTE_CONNECTION_LOST:
						HQRemote::Log("* Remote peer lost connection.\n");
						onConnected(packet->systemAddress, packet->guid, false);
						break;
					case ID_DISCONNECTION_NOTIFICATION:
						HQRemote::Log("* A peer has disconnected.\n");
						onConnected(packet->systemAddress, packet->guid, false);
						break;
					case ID_CONNECTION_LOST:
						HQRemote::Log("* A connection was lost.\n");
						onConnected(packet->systemAddress, packet->guid, false);
						break;
					case ID_CONNECTION_ATTEMPT_FAILED:
						HQRemote::Log("* A connection attempt failed.\n");
						onConnected(packet->systemAddress, packet->guid, false);
						break;
					default:
						break;
				} // check package identifier

				processPacket(packet);//sub-class's responsibility
			} // package receive loop

			//check if there are any multicast packets
			pollingMulticastData();

			//addtional update procedure
			additionalUpdateProc();
		}

		void ConnectionHandlerRakNet::pollingCallback(RakNet::RakPeerInterface* peer, void* userData) {
			auto _thiz = static_cast<ConnectionHandlerRakNet*>(userData);
			_thiz->pollingProc(peer);
		}

		// ---------- implements UDPProxyClientResultHandler ---------------
		void ConnectionHandlerRakNet::OnForwardingSuccess(const char *proxyIPAddress, unsigned short proxyPort,
														  RakNet::SystemAddress proxyCoordinator,
														  RakNet::SystemAddress sourceAddress,
														  RakNet::SystemAddress targetAddress,
														  RakNet::RakNetGUID targetGuid,
														  RakNet::UDPProxyClient *proxyClientPlugin)  {
			LogStream ss;

			ss << "ConnectionHandlerRakNet::OnForwardingSuccess: proxyIPAddress=" << proxyIPAddress
			   << " targetAddress=" << targetAddress
			   << " targetGuid=" << targetGuid
			   << '\n';

		}

		void ConnectionHandlerRakNet::OnForwardingNotification(const char *proxyIPAddress, unsigned short proxyPort,
															   RakNet::SystemAddress proxyCoordinator,
															   RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress,
															   RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			LogStream ss;

			ss << "ConnectionHandlerRakNet::OnForwardingNotification: proxyIPAddress=" << proxyIPAddress
			   << " targetAddress=" << targetAddress
			   << " targetGuid=" << targetGuid
			   << '\n';
		}

		void ConnectionHandlerRakNet::OnNoServersOnline(RakNet::SystemAddress proxyCoordinator, RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress, RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			LogStream ss;

			ss << "ConnectionHandlerRakNet::OnNoServersOnline: proxyCoordinator=" << proxyCoordinator
			   << " targetAddress=" << targetAddress
			   << " targetGuid=" << targetGuid
			   << '\n';
		}

		void ConnectionHandlerRakNet::OnRecipientNotConnected(RakNet::SystemAddress proxyCoordinator, RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress, RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			LogStream ss;

			ss << "ConnectionHandlerRakNet::OnRecipientNotConnected: proxyCoordinator=" << proxyCoordinator
			   << " targetAddress=" << targetAddress
			   << " targetGuid=" << targetGuid
			   << '\n';
		}

		void ConnectionHandlerRakNet::OnAllServersBusy(RakNet::SystemAddress proxyCoordinator, RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress, RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			LogStream ss;

			ss << "ConnectionHandlerRakNet::OnAllServersBusy: proxyCoordinator=" << proxyCoordinator
			   << " targetAddress=" << targetAddress
			   << " targetGuid=" << targetGuid
			   << '\n';
		}

		void ConnectionHandlerRakNet::OnForwardingInProgress(const char *proxyIPAddress, unsigned short proxyPort, RakNet::SystemAddress proxyCoordinator, RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress, RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			LogStream ss;

			ss << "ConnectionHandlerRakNet::OnForwardingInProgress: proxyCoordinator=" << proxyCoordinator
			   << " targetAddress=" << targetAddress
			   << " targetGuid=" << targetGuid
			   << '\n';
		}

		/*--------   ConnectionHandlerRakNetServer ------*/
		ConnectionHandlerRakNetServer::ConnectionHandlerRakNetServer(
				const RakNet::RakNetGUID* myGUID,
			    const char* natPunchServerAddress, int natPunchServerPort,
				const char* proxyServerAddress, int proxyServerPort,
				MasterServerConnectedCallback callback)
		: ConnectionHandlerRakNetServer(myGUID, natPunchServerAddress, natPunchServerPort, proxyServerAddress, proxyServerPort, nullptr, callback)
		{}

		ConnectionHandlerRakNetServer::ConnectionHandlerRakNetServer(
				const RakNet::RakNetGUID* myGUID,
				const char* natPunchServerAddress, int natPunchServerPort,
				const char* proxyServerAddress, int proxyServerPort,
				PublicServerMetaDataGenerator publicServerMetaGenerator,
				MasterServerConnectedCallback callback)
		:ConnectionHandlerRakNet(myGUID, natPunchServerAddress, natPunchServerPort, proxyServerAddress, proxyServerPort, DEFAULT_SERVER_PORT, DEFAULT_MAX_INCOMING_CONNECTIONS, true, callback),
		m_reconnectionWaitStartTime(0), m_invitationKey(0), m_dontWait(false), m_autoGenKey(true),
		 m_publicServerMetaGenerator(publicServerMetaGenerator)
		{}

		ConnectionHandlerRakNetServer::ConnectionHandlerRakNetServer(
			const RakNet::RakNetGUID* myGUID,
			const char* natPunchServerAddress, int natPunchServerPort,
			const char* proxyServerAddress, int proxyServerPort,
			uint64_t fixedInvitationKey,
			PublicServerMetaDataGenerator publicServerMetaGenerator,
			MasterServerConnectedCallback callback)
		: ConnectionHandlerRakNet(myGUID, natPunchServerAddress, natPunchServerPort, proxyServerAddress, proxyServerPort, DEFAULT_SERVER_PORT, DEFAULT_MAX_INCOMING_CONNECTIONS, true, callback),
			m_reconnectionWaitStartTime(0), m_invitationKey(fixedInvitationKey), m_dontWait(false), m_autoGenKey(false),
			m_publicServerMetaGenerator(publicServerMetaGenerator)
		{}
		void ConnectionHandlerRakNetServer::createNewInvitation() {
			std::lock_guard<std::mutex> lg(m_sendingLock);

			//do the work on update thread
			unsigned char restartId = ID_USER_PACKET_REINVITE_FRIEND;
			m_rakPeer->SendLoopback((char*)&restartId, sizeof(restartId));
		}

		void ConnectionHandlerRakNetServer::stopExImpl() {
			m_reconnectionWaitStartTime = 0;
			if (m_autoGenKey)
				m_invitationKey = 0;
		}

		void ConnectionHandlerRakNetServer::onNatServerConnected(bool connected) {
			if (!connected)
				return;
			if (!acceptIncomingConnection())
				setInternalError("Failed to accept incoming connection");

			if (m_autoGenKey)
				m_invitationKey = generateInvitationKey();//random generate invitation key

			// make this server public?
			if (m_publicServerMetaGenerator) {
				auto meta = m_publicServerMetaGenerator(this);

				// | game_id | meta_length | meta
				RakNet::BitStream bs;

				uint32_t meta_length = static_cast<uint32_t>(meta.size());
				if (meta_length == 0)
					return;

				bs.Write(static_cast<unsigned char>(ID_USER_PACKET_CREATE_PUBLIC_ROOM));
				bs.Write(THIS_GAME_ID);
				bs.Write(meta_length);
				bs.Write(meta.c_str(), meta_length);

				m_rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 1, m_natPunchServerAddress, false);
			}
		}

		uint64_t ConnectionHandlerRakNetServer::generateInvitationKey() {
			return RakNet::RakPeerInterface::Get64BitUniqueRandomNumber();
		}

		void ConnectionHandlerRakNetServer::onPeerConnected(RakNet::SystemAddress address, RakNet::RakNetGUID guid, bool connected) {
			if (!connected) {
				if(address == m_remotePeerAddress)
				{
					if (m_connected)
					{
						m_reconnectionWaitStartTime = HQRemote::getTimeCheckPoint64();//wait for client to reconnect
					}
				}
			}
			else {
				//client has reconnected successfully
				if(m_connected && address == m_remotePeerAddress) {
					m_reconnectionWaitStartTime = 0;//reset timer
				}
			}
		}

		void ConnectionHandlerRakNetServer::additionalUpdateProc() {
			std::lock_guard<std::mutex> lg(m_sendingLock);//guard m_reconnectionWaitStartTime & m_connected

			if (m_reconnectionWaitStartTime != 0)
			{
				auto curTime = HQRemote::getTimeCheckPoint64();
				auto waitDuration = HQRemote::getElapsedTime64(m_reconnectionWaitStartTime, curTime);

				if (m_dontWait || waitDuration >= MAX_RECONNECTION_WAIT_DURATION) {
					//waited for too long and client hasn't reconnected yet, so drop it
					doRestart();
				}
			}//if (m_reconnectionWaitStartTime != 0)
		}

		void ConnectionHandlerRakNetServer::doRestart() {
			bool wasConnected = m_connected.load();
			m_connected = false;
			m_dontWait = false;

			m_reconnectionWaitStartTime = 0;//reset timer
			m_remotePeerAddress = RakNet::SystemAddress();//invalidate connected peer's address

			// IConnectionHandler::onDisconnected();
			if (wasConnected)
				onDisconnected(); // notify base class

			restart();
		}

		void ConnectionHandlerRakNetServer::processPacket(RakNet::Packet* packet) {
			switch (packet->data[0]) {
				case ID_USER_PACKET_ENUM_REQUEST_TO_JOIN:
				case ID_USER_PACKET_ENUM_REQUEST_TO_REJOIN:
				case ID_USER_PACKET_ENUM_TEST_CONNECTIVITY:
				{
					bool accept = false;
					uint64_t requestInvitationKey = 0;
					RakNet::BitStream bi(packet->data + 1, packet->length - 1, false);

					//read embedded invitation key
					bi.Read(requestInvitationKey);

					if (m_connected)//already has a connected peer
					{
						//only reconnection is accepted to proceed
						if (packet->data[0] == ID_USER_PACKET_ENUM_REQUEST_TO_REJOIN && packet->systemAddress == m_remotePeerAddress)
							accept = true;
					}
					else {
						//we have no existing connection. Reconnection request will not be allowed
						if (packet->data[0] != ID_USER_PACKET_ENUM_REQUEST_TO_REJOIN)
							accept = true;
					}

					//the embedded invitation key must match our invitation key
					accept = accept && requestInvitationKey == m_invitationKey;

					if (!accept)
					{
						//refuse
						RakNet::BitStream bs;
						bs.Write(static_cast<unsigned char>(ID_USER_PACKET_ENUM_REFUSED));

						m_rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 1, packet->systemAddress, false);
					}
					else {
						//accepted
						RakNet::BitStream bs;
						bs.Write(static_cast<unsigned char>(ID_USER_PACKET_ENUM_ACCEPTED));

						m_rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 1, packet->systemAddress, false);

						if (packet->data[0] == ID_USER_PACKET_ENUM_REQUEST_TO_JOIN ||
							packet->data[0] == ID_USER_PACKET_ENUM_REQUEST_TO_REJOIN)
							setActiveConnection(packet->systemAddress);
					}
				}
					break;
				case ID_UNCONNECTED_PING:
				{
					char addressStr[256];
					packet->systemAddress.ToString(true, addressStr);
					HQRemote::Log("* Got ping from %s\n", addressStr);
				}
					break;
				case ID_DISCONNECTION_NOTIFICATION:
				{
					if (packet->systemAddress == m_remotePeerAddress)
						m_dontWait = true;
				}
					break;
				case ID_USER_PACKET_REINVITE_FRIEND:
				{
					if (m_connected)
					{
						//close current connection
						m_rakPeer->CloseConnection(m_remotePeerAddress, true);

						m_dontWait = true;
					}
					else
						doRestart();
				}
					break;
				default:
					break;
			}
		}

		/*--------   ConnectionHandlerRakNetClient ------*/
		ConnectionHandlerRakNetClient::ConnectionHandlerRakNetClient(const RakNet::RakNetGUID* myGUID,
																	 const char* natPunchServerAddress, int natPunchServerPort,
																	 const char* proxyServerAddress, int proxyServerPort,
																	 const RakNet::RakNetGUID& remoteGUID,
																	 std::string&& remoteLanIp,
																	 unsigned short remoteLanPort,
																	 uint64_t remoteInvitationKey,
																	 bool testConnectivityOnly,
																	 MasterServerConnectedCallback callback)
		:ConnectionHandlerRakNet(myGUID, natPunchServerAddress, natPunchServerPort, proxyServerAddress, proxyServerPort, 0, 1, false, callback),
		m_remoteGUID(remoteGUID), m_remoteInvitationKey(remoteInvitationKey),
		m_remoteLanIpString(std::move(remoteLanIp)), m_remoteLanPort(remoteLanPort),
		m_testOnly(testConnectivityOnly), m_testCallback(nullptr),
		m_remainReconnectionAttempts(0),
		m_tryConnectToLanIp(false),
		m_onStopCallback(nullptr)
		{
			if (m_remoteLanIpString.size() != 0 && m_remoteLanPort != 0)
				m_remoteLanAddress = RakNet::SystemAddress(m_remoteLanIpString.c_str(), m_remoteLanPort);
		}

		bool ConnectionHandlerRakNetClient::isConnectedThroughProxy() const {
			return m_connected && m_remotePeerAddress != RakNet::UNASSIGNED_SYSTEM_ADDRESS
				   && m_remotePeerAddress == m_remoteProxyAddress;
		}

		ConnectionHandlerRakNetClient& ConnectionHandlerRakNetClient::setTestingConnectivityCallback(ConnectivityResultCallback callback) {
			if (!m_testOnly)
				return *this;

			m_testCallback = callback;

			return *this;
		}

		ConnectionHandlerRakNetClient& ConnectionHandlerRakNetClient::setOnStopCallback(OnStopCallback callback) {
			m_onStopCallback = callback;
			return *this;
		}

		void ConnectionHandlerRakNetClient::stopExImpl() {
			m_remainReconnectionAttempts = 0;
			m_tryConnectToLanIp = false;

			if (m_onStopCallback)
				m_onStopCallback(this);
		}

		void ConnectionHandlerRakNetClient::onNatServerConnected(bool connected) {
			if (!connected)
			{
				return;
			}

			acceptIncomingConnection();//needed by Ping reply

			if (!attemptNatPunchthrough(m_remoteGUID))
				setInternalError("Failed to establish peer connection");//TODO: localize

			m_remainReconnectionAttempts = MAX_RECONNECTION_ATTEMPTS;//allow peer reconnection in future
		}

		void ConnectionHandlerRakNetClient::tryLanConnection() {
			// try to connect to LAN Ip address
#if !SKIP_LAN_CONNECTION
			if (m_remoteLanIpString.size() && !m_tryConnectToLanIp)
			{
				m_tryConnectToLanIp = true;

				HQRemote::Log("* Connecting to the peer through LAN address %s:%u ...\n", m_remoteLanIpString.c_str(), m_remoteLanPort);
				m_rakPeer->Connect(m_remoteLanIpString.c_str(),
								   m_remoteLanPort, 0, 0);
			}
			else
#endif // SKIP_LAN_CONNECTION
			{
				tryRelayConnection(false);
			}
		}

		void ConnectionHandlerRakNetClient::tryRelayConnection(bool hasSendingLock) {
			if (m_proxyCoordinatorAddress == RakNet::UNASSIGNED_SYSTEM_ADDRESS) {
				// no proxy server? use LAN discovery approach next
				tryDiscoverLanServer(hasSendingLock);
				return;
			}

			HQRemote::Log("* Attempt to use proxy server\n");

			m_proxyClient.RequestForwarding(
					m_proxyCoordinatorAddress,
					RakNet::UNASSIGNED_SYSTEM_ADDRESS,
					m_remoteGUID,
					DEFAULT_RELAY_ATTEMPT_TIMEOUT_MS
			);
		}

		bool ConnectionHandlerRakNetClient::tryReconnect(RakNet::SystemAddress address) {
			//TODO: reliable data won't be resent if it is lost during connection loss
			if (m_remainReconnectionAttempts == 0)
				return false;

			char addressStr[256];
			address.ToString(false, addressStr);

			HQRemote::Log("* Attempt to reconnect to %s:%d\n", addressStr, (int)address.GetPort());

			auto connectRe = m_rakPeer->Connect(addressStr, address.GetPort(), 0, 0);

			if (connectRe != RakNet::CONNECTION_ATTEMPT_STARTED &&
				connectRe != RakNet::CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS &&
				connectRe != RakNet::ALREADY_CONNECTED_TO_ENDPOINT)
				return false;

			if (connectRe == RakNet::CONNECTION_ATTEMPT_STARTED)
				m_remainReconnectionAttempts --;

			return true;
		}

		bool ConnectionHandlerRakNetClient::tryDiscoverLanServer(bool hasSendingLock) {
			HQRemote::Log("* Trying to discover host in LAN using broadcast and multicast ping ...\n");

			//retry with lan discovery. Use both broadcast ping & multicast ping
			m_rakPeer->Ping("255.255.255.255", DEFAULT_SERVER_PORT, false);

			return pingMulticastGroup(hasSendingLock);
		}

		void ConnectionHandlerRakNetClient::onPeerConnected(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid, bool connected) {
			if (guid == m_remoteGUID || (m_connected && connectedAddress == m_remotePeerAddress))
			{
				if (connected)
				{
					RakNet::BitStream bs;
					bs.Write(static_cast<unsigned char>(m_testOnly ?
														ID_USER_PACKET_ENUM_TEST_CONNECTIVITY :
														m_connected ? ID_USER_PACKET_ENUM_REQUEST_TO_REJOIN : ID_USER_PACKET_ENUM_REQUEST_TO_JOIN));

					//embed the invitation key
					bs.Write(m_remoteInvitationKey);

					//request server to join/test the connectivity of the game
					m_rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 1, connectedAddress, false);

					m_remainReconnectionAttempts = MAX_RECONNECTION_ATTEMPTS;//allow future reconnection attempts
				}
				else {
					if (m_testOnly || !tryReconnect(connectedAddress))
					{
						bool wasConnected = m_connected.load();

						m_connected = false;

						// IConnectionHandler::onDisconnected();
						if (wasConnected && !m_testOnly)
							onDisconnected(); // notify base class

						m_remotePeerAddress = RakNet::SystemAddress();//invalidate connected peer's address
					}
				}
			} else if (m_tryConnectToLanIp && connectedAddress == m_remoteLanAddress) {
				// attempt to connect to supplied LAN address failed
				m_tryConnectToLanIp = false;

				HQRemote::Log("* Connecting to the peer through LAN address %s failed\n", m_remoteLanIpString.c_str());

				// try the relay approach
				tryRelayConnection(true);
			} else if (connectedAddress == m_remoteProxyAddress) {
				// is this proxy address's first attempt? then try discover LAN server next
				m_remoteProxyAddress = RakNet::UNASSIGNED_SYSTEM_ADDRESS;

				tryDiscoverLanServer(true);
			}
		}

		void ConnectionHandlerRakNetClient::onPeerUnreachable(RakNet::SystemAddress connectedAddress, RakNet::RakNetGUID guid) {
			if (guid == m_remoteGUID)
			{
				setInternalError("Unreachable peer. Probably because the game session already has enough players or has ended.");//TODO: localize
			}
		}

		void ConnectionHandlerRakNetClient::processPacket(RakNet::Packet* packet) {
			switch (packet->data[0]) {
				case ID_USER_PACKET_ENUM_ACCEPTED:
					if (packet->guid == m_remoteGUID)
					{
						HQRemote::Log("* Connection accepted\n");

						if (m_testOnly)
						{
							//close connection
							m_rakPeer->CloseConnection(packet->systemAddress, true);

							if (m_testCallback)
								m_testCallback(this, true);
						}
						else
						{
							//set this as active peer for exchanging data
							setActiveConnection(packet->systemAddress);
						}
					}
					break;
				case ID_USER_PACKET_ENUM_REFUSED:
					if (packet->guid == m_remoteGUID)
					{
						HQRemote::Log("* Connection refused\n");

						//close connection
						m_rakPeer->CloseConnection(packet->systemAddress, true);

						m_remainReconnectionAttempts = 0;//no more attempt

						onPeerConnected(packet->systemAddress, packet->guid, false);
						onPeerUnreachable(packet->systemAddress, packet->guid);

						if (m_testOnly && m_testCallback) {
							m_testCallback(this, false);
						}
					}
					break;
					/// NATPunchthrough plugin: This message is generated on the local system, and does not come from the network.
					///  packet::guid contains the destination field of NatPunchthroughClient::OpenNAT(). Byte 1 contains 1 if you are the sender, 0 if not
				case ID_NAT_PUNCHTHROUGH_FAILED:
				{
					char addressStr[128];
					packet->systemAddress.ToString(false, addressStr);

					HQRemote::Log("* NAT punchthrough error id=%d address=%s\n", (int)packet->data[1], addressStr);
					if (packet->data[1])//sender
					{
						tryLanConnection();
					}

				}
					break;
				case ID_UNCONNECTED_PONG:
				{
					//we found our server inside LAN by using ping broadcasting, connect to it
					if (packet->guid == m_remoteGUID && m_sendingNATPunchthroughRequest) {
						m_sendingNATPunchthroughRequest = false;

						char addressStr[128];
						packet->systemAddress.ToString(false, addressStr);

						HQRemote::Log("* Connecting to the peer %s ...\n", addressStr);
						m_rakPeer->Connect(addressStr,
										   packet->systemAddress.GetPort(), 0, 0);
					}
				}
					break;
				case ID_ADVERTISE_SYSTEM:
				{
					//we found our server inside LAN, connect to it
					RakNet::RakNetGUID srcGuid;
					memcpy(&srcGuid.g, packet->data + 1, sizeof(srcGuid.g));

					srcGuid.g = ntohll(srcGuid.g);

					if (srcGuid == m_remoteGUID && m_sendingNATPunchthroughRequest) {
						m_sendingNATPunchthroughRequest = false;

						char addressStr[128];
						packet->systemAddress.ToString(false, addressStr);

						HQRemote::Log("* Connecting to the peer %s ...\n", addressStr);
						m_rakPeer->Connect(addressStr,
										   packet->systemAddress.GetPort(), 0, 0);
					}
				}
					break;
				case ID_DISCONNECTION_NOTIFICATION:
				{
					//server has forcefully close the connection, so don't attempt to reconnect
					if (packet->guid == m_remoteGUID)
						m_remainReconnectionAttempts = 0;//no more attempt
				}
					break;
				default:
					break;
			}
		}

		/// Called when our forwarding request was completed. We can now connect to \a targetAddress by using \a proxyAddress instead
		/// \param[out] proxyIPAddress IP Address of the proxy server, which will forward messages to targetAddress
		/// \param[out] proxyPort Remote port to use on the proxy server, which will forward messages to targetAddress
		/// \param[out] proxyCoordinator \a proxyCoordinator parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] sourceAddress \a sourceAddress parameter passed to UDPProxyClient::RequestForwarding. If it was UNASSIGNED_SYSTEM_ADDRESS, it is now our external IP address.
		/// \param[out] targetAddress \a targetAddress parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] targetGuid \a targetGuid parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] proxyClient The plugin that is calling this callback
		void ConnectionHandlerRakNetClient::OnForwardingSuccess(const char *proxyIPAddress, unsigned short proxyPort,
																RakNet::SystemAddress proxyCoordinator,
																RakNet::SystemAddress sourceAddress,
																RakNet::SystemAddress targetAddress,
																RakNet::RakNetGUID targetGuid,
																RakNet::UDPProxyClient *proxyClientPlugin) {
			ConnectionHandlerRakNet::OnForwardingSuccess(proxyIPAddress, proxyPort, proxyCoordinator, sourceAddress, targetAddress, targetGuid, proxyClientPlugin);

			HQRemote::Log("* Connecting to the peer through proxy address %s:%u ...\n", proxyIPAddress, proxyPort);
			m_rakPeer->Connect(proxyIPAddress,
							   proxyPort, 0, 0);

			m_remoteProxyAddress = RakNet::SystemAddress(proxyIPAddress, proxyPort);
		}

		/// Called when our forwarding request failed, because no UDPProxyServers are connected to UDPProxyCoordinator
		/// \param[out] proxyCoordinator \a proxyCoordinator parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] sourceAddress \a sourceAddress parameter passed to UDPProxyClient::RequestForwarding. If it was UNASSIGNED_SYSTEM_ADDRESS, it is now our external IP address.
		/// \param[out] targetAddress \a targetAddress parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] targetGuid \a targetGuid parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] proxyClient The plugin that is calling this callback
		void ConnectionHandlerRakNetClient::OnNoServersOnline(RakNet::SystemAddress proxyCoordinator, RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress, RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			ConnectionHandlerRakNet::OnNoServersOnline(proxyCoordinator, sourceAddress, targetAddress, targetGuid, proxyClientPlugin);

			// try to discover whether the remote host is in the same LAN
			// by sending broadcast and multicast ping
			tryDiscoverLanServer(false);
		}

		/// Called when our forwarding request failed, because no UDPProxyServers are connected to UDPProxyCoordinator
		/// \param[out] proxyCoordinator \a proxyCoordinator parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] sourceAddress \a sourceAddress parameter passed to UDPProxyClient::RequestForwarding. If it was UNASSIGNED_SYSTEM_ADDRESS, it is now our external IP address.
		/// \param[out] targetAddress \a targetAddress parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] targetGuid \a targetGuid parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] proxyClient The plugin that is calling this callback
		void ConnectionHandlerRakNetClient::OnRecipientNotConnected(RakNet::SystemAddress proxyCoordinator, RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress, RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			ConnectionHandlerRakNet::OnRecipientNotConnected(proxyCoordinator, sourceAddress, targetAddress, targetGuid, proxyClientPlugin);

			// try to discover whether the remote host is in the same LAN
			// by sending broadcast and multicast ping
			tryDiscoverLanServer(false);
		}

		/// Called when our forwarding request failed, because all UDPProxyServers that are connected to UDPProxyCoordinator are at their capacity
		/// Either add more servers, or increase capacity via UDPForwarder::SetMaxForwardEntries()
		/// \param[out] proxyCoordinator \a proxyCoordinator parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] sourceAddress \a sourceAddress parameter passed to UDPProxyClient::RequestForwarding. If it was UNASSIGNED_SYSTEM_ADDRESS, it is now our external IP address.
		/// \param[out] targetAddress \a targetAddress parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] targetGuid \a targetGuid parameter originally passed to UDPProxyClient::RequestForwarding
		/// \param[out] proxyClient The plugin that is calling this callback
		void ConnectionHandlerRakNetClient::OnAllServersBusy(RakNet::SystemAddress proxyCoordinator, RakNet::SystemAddress sourceAddress, RakNet::SystemAddress targetAddress, RakNet::RakNetGUID targetGuid, RakNet::UDPProxyClient *proxyClientPlugin) {
			ConnectionHandlerRakNet::OnAllServersBusy(proxyCoordinator, sourceAddress, targetAddress, targetGuid, proxyClientPlugin);

			// try to discover whether the remote host is in the same LAN
			// by sending broadcast and multicast ping
			tryDiscoverLanServer(false);
		}

		/*------------- RakNetValidGuidChecker --------------*/
		RakNetValidGuidChecker::RakNetValidGuidChecker(const RakNet::RakNetGUID* myGUID,
													   const char* natPunchServerAddress,
													   int natPunchServerPort)
		:ConnectionHandlerRakNet(myGUID, natPunchServerAddress, natPunchServerPort, nullptr, 0, 0, 1, false),
		 invalidGuidsNotificationCallback(nullptr)
		{
		}

		void RakNetValidGuidChecker::checkIfGuidsValid(const RakNet::RakNetGUID* guids, size_t _numGuidsToCheck) {
			std::lock_guard<std::mutex> lg(m_pendingDataLock);

			if (_numGuidsToCheck > MAX_GUIDS_TO_CHECK)
				_numGuidsToCheck = MAX_GUIDS_TO_CHECK;

			auto bs = std::make_shared<RakNet::BitStream>();
			bs->Write(static_cast<unsigned char>(ID_USER_PACKET_ENUM_CHECK_GUID));
			bs->Write(static_cast<unsigned int>(_numGuidsToCheck));

			for (size_t i = 0; i < _numGuidsToCheck; ++i) {
				bs->Write(guids[i]);
			}

			//if connected to NAT server, send the query immediately, otherwise buffer it
			if (m_natServerConnected)
				m_rakPeer->Send(bs.get(), HIGH_PRIORITY, RELIABLE_ORDERED, 1, m_natPunchServerAddress, false);
			else
			{
				m_pendingDataToSend.push_back(bs);
			}
		}

		void RakNetValidGuidChecker::onNatServerConnected(bool connected) {
			if (connected) {
				//send the pending request data to NAT server
				std::lock_guard<std::mutex> lg(m_pendingDataLock);

				for (auto &data : m_pendingDataToSend) {
					m_rakPeer->Send(data.get(), HIGH_PRIORITY, RELIABLE_ORDERED, 1, m_natPunchServerAddress, false);
				}
			}
		}

		void RakNetValidGuidChecker::processPacket(RakNet::Packet* packet) {
			switch (packet->data[0]) {
				case ID_USER_PACKET_ENUM_INVALID_GUID_LIST:
				{
					//NAT server has returned use the list of invalid GUIDs
					RakNet::BitStream bi(packet->data + 1, packet->length - 1, false);

					RakNet::RakNetGUID invalidGuids[MAX_GUIDS_TO_CHECK];

					unsigned int numInvalidGuids;

					bi.Read(numInvalidGuids);

					if (numInvalidGuids > MAX_GUIDS_TO_CHECK)
						numInvalidGuids = MAX_GUIDS_TO_CHECK;

					for (unsigned int i = 0; i < numInvalidGuids; ++i) {
						bi.Read(invalidGuids[i]);
					}

					//notify observer
					if (invalidGuidsNotificationCallback)
						invalidGuidsNotificationCallback(this, invalidGuids, numInvalidGuids);
				}
				break;
			}//switch (packet->data[0])
		}
	}
}