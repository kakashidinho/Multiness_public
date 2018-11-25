////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2016-2018 Le Hoang Quyen
// Copyright (C) 2003-2008 Martin Freij
//
// This file is part of Nestopia.
//
// Nestopia is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Nestopia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Nestopia; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef NST_API_MACHINE_H
#define NST_API_MACHINE_H

#include <iosfwd>
#include "NstApi.hpp"

#include <string>
#include <memory>

#ifdef NST_PRAGMA_ONCE
#pragma once
#endif

#if NST_ICC >= 810
#pragma warning( push )
#pragma warning( disable : 444 )
#elif NST_MSVC >= 1200
#pragma warning( push )
#pragma warning( disable : 4512 )
#endif

namespace HQRemote {
	class IConnectionHandler;
}

namespace Nes
{
	namespace Api
	{
		/**
		* Machine interface.
		*/
		class Machine : public Base
		{
			struct EventCaller;

		public:

			/**
			* Interface constructor.
			*
			* @param instance emulator instance
			*/
			template<typename T>
			Machine(T& instance)
			: Base(instance) {}

			enum
			{
				ON        = 0x001,
				VS        = 0x010,
				PC10      = 0x020,
				CARTRIDGE = 0x040,
				DISK      = 0x080,
				SOUND     = 0x100,
				REMOTE	  = 0x200,
				GAME      = CARTRIDGE|DISK|REMOTE,
				IMAGE     = GAME|SOUND
			};

			/**
			* NTSC/PAL mode.
			*/
			enum Mode
			{
				/**
				* NTSC.
				*/
				NTSC = 0x04,
				/**
				* PAL.
				*/
				PAL = 0x08
			};

			/**
			* Favored System.
			*
			* Used for telling what console to emulate if the core can't decide by itself.
			*/
			enum FavoredSystem
			{
				/**
				* NES NTSC.
				*/
				FAVORED_NES_NTSC = Core::FAVORED_NES_NTSC,
				/**
				* NES PAL.
				*/
				FAVORED_NES_PAL = Core::FAVORED_NES_PAL,
				/**
				* Famicom.
				*/
				FAVORED_FAMICOM = Core::FAVORED_FAMICOM,
				/**
				* Dendy (clone).
				*/
				FAVORED_DENDY = Core::FAVORED_DENDY
			};

			/**
			* Image profile questioning state.
			*
			* Used for allowing callback triggering if an image has multiple media profiles.
			*/
			enum AskProfile
			{
				/**
				* Don't trigger callback (default).
				*/
				DONT_ASK_PROFILE,
				/**
				* Trigger callback.
				*/
				ASK_PROFILE
			};

			enum
			{
				CLK_NTSC_DOT   = Core::CLK_NTSC,
				CLK_NTSC_DIV   = Core::CLK_NTSC_DIV,
				CLK_NTSC_VSYNC = Core::PPU_RP2C02_HVSYNC * ulong(Core::CLK_NTSC_DIV),
				CLK_PAL_DOT    = Core::CLK_PAL,
				CLK_PAL_DIV    = Core::CLK_PAL_DIV,
				CLK_PAL_VSYNC  = Core::PPU_RP2C07_HVSYNC * ulong(Core::CLK_PAL_DIV)
			};

			enum
			{
				MAX_REMOTE_MESSAGE_SIZE = 256
			};

			/**
			* Soft-patching context object.
			*
			* Used as input parameter to some of the image loading methods.
			*/
			struct Patch
			{
				/**
				* Input stream containing the patch in UPS or IPS format.
				*/
				std::istream& stream;

				/**
				* Set to true to bypass checksum validation.
				*/
				bool bypassChecksum;

				/**
				* Will contain the result of the operation after the image has been loaded.
				*/
				Result result;

				/**
				* Constructor.
				*
				* @param s input stream
				* @param b true to bypass checksum validation, default is false
				*/
				Patch(std::istream& s,bool b=false)
				: stream(s), bypassChecksum(b), result(RESULT_NOP) {}
			};

			/**
			* Loads any image. Input stream can be in XML, iNES, UNIF, FDS or NSF format.
			*
			* @param stream input stream containing the image to load
			* @param system console to emulate if the core can't do automatic detection
			* @param askProfile to allow callback triggering if the image has multiple media profiles, default is false
			* @return result code
			*/
			Result Load(std::istream& stream,FavoredSystem system,AskProfile askProfile=DONT_ASK_PROFILE) throw();

			/**
			* Loads any image. Input stream can be in XML, iNES, UNIF, FDS or NSF format.
			*
			* @param stream input stream containing the image to load
			* @param system console to emulate if the core can't do automatic detection
			* @param patch object for performing soft-patching on the image
			* @param askProfile to allow callback triggering if the image has multiple media profiles, default is false
			* @return result code
			*/
			Result Load(std::istream& stream,FavoredSystem system,Patch& patch,AskProfile askProfile=DONT_ASK_PROFILE) throw();

			/**
			* Loads a cartridge image. Input stream can be in XML, iNES or UNIF format.
			*
			* @param stream input stream containing the image to load
			* @param system console to emulate if the core can't do automatic detection
			* @param askProfile to allow callback triggering if the image has multiple media profiles, default is false
			* @return result code
			*/
			Result LoadCartridge(std::istream& stream,FavoredSystem system,AskProfile askProfile=DONT_ASK_PROFILE) throw();

			/**
			* Loads a cartridge image. Input stream can be in XML, iNES or UNIF format.
			*
			* @param stream input stream containing the image to load
			* @param system console to emulate if the core can't do automatic detection
			* @param patch object for performing soft-patching on the image
			* @param askProfile to allow callback triggering if the image has multiple media profiles, default is false
			* @return result code
			*/
			Result LoadCartridge(std::istream& stream,FavoredSystem system,Patch& patch,AskProfile askProfile=DONT_ASK_PROFILE) throw();

			/**
			* Loads a Famicom Disk System image. Input stream can be in FDS format.
			*
			* @param stream input stream containing the image to load
			* @param system console to emulate if the core can't do automatic detection
			* @return result code
			*/
			Result LoadDisk(std::istream& stream,FavoredSystem system) throw();

			/**
			* Loads a sound image. Input stream can be in NSF format.
			*
			* @param stream input stream containing the image to load
			* @param system console to emulate if the core can't do automatic detection
			* @return result code
			*/
			Result LoadSound(std::istream& stream,FavoredSystem system) throw();

			/**
			* Unloads the current image.
			*
			* @return result code
			*/
			Result Unload() throw();

			//LHQ
			Result LoadRemote(std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* clientName = NULL) throw();
			Result LoadRemote(const std::string& remoteIp, int remotePort, const char* clientName = NULL) throw();

			Result EnableRemoteController(uint idx, std::shared_ptr<HQRemote::IConnectionHandler> connHandler, const char* hostName = NULL) throw();
			Result EnableRemoteController(uint idx, int networkListenPort, const char* hostName = NULL) throw();

			//get connection handler which was passed to EnableRemoteController()
			std::shared_ptr<HQRemote::IConnectionHandler> GetRemoteControllerHostConnHandler();
			std::shared_ptr<HQRemote::IConnectionHandler> GetRemoteControllerClientConnHandler();

			void DisableRemoteController(uint idx);
			bool RemoteControllerConnected(uint idx) const;
			bool RemoteControllerEnabled(uint idx) const;

			void EnableLowResRemoteControl(bool enable);

			//<id> is used for ACK message later to acknowledge that the message is received by remote side.
			//<message> must not have more than MAX_REMOTE_MESSAGE_SIZE bytes (excluding NULL character). Otherwise RESULT_ERR_BUFFER_TOO_BIG is retuned.
			//If there is no remote connection, RESULT_ERR_NOT_READY is returned
			//This function can be used to send message between client & server.
			Result SendMessageToRemote(uint64_t id, const char* message);

			//<idx> is ignored if machine is in client mode
			const char* GetRemoteName(uint remoteCtlIdx) const;
			//end LHQ

			/**
			* Powers ON or OFF the machine.
			*
			* @param state ON if true
			* @return result code
			*/
			Result Power(bool state) throw();

			/**
			* Resets the machine.
			*
			* @param state hard-reset if true, soft-reset otherwise
			* @return result code
			*/
			Result Reset(bool state) throw();

			/**
			* Returns the current mode.
			*
			* @return mode
			*/
			Mode GetMode() const throw();

			/**
			* Returns the mode most appropriate for the current image.
			*
			* @return mode
			*/
			Mode GetDesiredMode() const throw();

			/**
			* Sets the mode.
			*
			* @param mode new mode
			* @return result code
			*/
			Result SetMode(Mode mode) throw();

			/**
			* Internal compression on states.
			*/
			enum Compression
			{
				/**
				* No compression.
				*/
				NO_COMPRESSION,
				/**
				* Compression enabled (default).
				*/
				USE_COMPRESSION
			};

			/**
			* Loads a state.
			*
			* @param stream input stream containing the state
			* @return result code
			*/
			Result LoadState(std::istream& stream) throw();

			/**
			* Saves a state.
			*
			* @param stream output stream which the state will be written to
			* @param compression to allow internal compression in the state, default is USE_COMPRESSION
			* @return result code
			*/
			Result SaveState(std::ostream& stream,Compression compression=USE_COMPRESSION) const throw();

			/**
			* Returns a machine state.
			*
			* @param flags OR:ed flags to check
			* @return OR:ed flags evaluated to true
			*/
			uint Is(uint flags) const throw();

			/**
			* Returns a machine state.
			*
			* @param flags1 OR:ed flags to check
			* @param flags2 OR:ed flags to check
			* @return true if <b>both</b> parameters has at least one flag evaluated to true
			*/
			bool Is(uint flags1,uint flags2) const throw();

			/**
			* Tells if the machine is in a locked state.
			*
			* A locked state means that the machine can't respond to
			* certain operations because it's doing something special,
			* like playing a movie or rewinding.
			*
			* @return true if machine is in a locked state.
			*/
			bool IsLocked() const;

			/*
			* Used by EVENT_REMOTE_MESSAGE
			*/
			struct RemoteMsgEventData {
				int senderIdx;//<senderIdx> = -1 if sender is the host, remote controller index if sender is client
				const char* message;
			};

			/**
			* Machine events.
			*/
			enum Event
			{
				/**
				* A new image has been loaded into the system.
				*/
				EVENT_LOAD,
				/**
				* Connection to remote side established or failed with error.
				*/
				EVENT_LOAD_REMOTE,
				/**
				 * Remote controller enabled, result value is controller index or an error code
				 */
				EVENT_REMOTE_CONTROLLER_ENABLED,
				/**
				 * Remote controller disabled, result value is controller index
				 */
				EVENT_REMOTE_CONTROLLER_DISABLED,
				/**
				 * Connection to remote side had some errors. result value is address of error string
				 */
				EVENT_REMOTE_CONNECTION_INTERNAL_ERROR,
				/**
				* Connection to remote side finished successfully. result value is address of host name's string (optional)
				*/
				EVENT_REMOTE_CONNECTED,
				/**
				* Connection to remote side disconnected or timeout.
				*/
				EVENT_REMOTE_DISCONNECTED,
				/**
				* A client has connected. result value is address of client info's string
				*/
				EVENT_CLIENT_CONNECTED,
				/**
				* A client has disconnected. result value is address of client info's string
				*/
				EVENT_CLIENT_DISCONNECTED,
				/**
				* Remote controlling will use low resolution image.
				*/
				EVENT_REMOTE_LOWRES,
				/*
				* Note: result value is address of the floating point data representing the data rate
				*/
				EVENT_REMOTE_DATA_RATE,
				/*
				* Message received from remote side. result value is pointer points to a RemoteMsgEventData struct
				*/
				EVENT_REMOTE_MESSAGE,
				/*
				* Sent message received by remote side. result value is pointer to id of message
				*/
				EVENT_REMOTE_MESSAGE_ACK,
				/**
				* An image has been unloaded from the system.
				*/
				EVENT_UNLOAD,
				/**
				* Machine power ON.
				*/
				EVENT_POWER_ON,
				/**
				* Machine power OFF.
				*/
				EVENT_POWER_OFF,
				/**
				* Machine soft-reset.
				*/
				EVENT_RESET_SOFT,
				/**
				* Machine hard-reset.
				*/
				EVENT_RESET_HARD,
				/**
				* Mode has changed to NTSC.
				*/
				EVENT_MODE_NTSC,
				/**
				* Mode has changed to PAL.
				*/
				EVENT_MODE_PAL
			};

			enum
			{
				NUM_EVENT_CALLBACKS = 8
			};

			/**
			* Machine event callback prototype.
			*
			* @param userData optional user data
			* @param event the event
			* @param result result code of event operation, in success or warning state
			*/
			typedef void (NST_CALLBACK *EventCallback) (UserData userData,Event event,Result result);

			/**
			* Machine event callback manager.
			*
			* Static object used for adding the user defined callback.
			*/
			static EventCaller eventCallback;

		private:

			Result Load(std::istream&,FavoredSystem,AskProfile,Patch*,uint);
		};

		/**
		* Machine event callback invoker.
		*
		* Used internally by the core.
		*/
		struct Machine::EventCaller : Core::UserCallback<Machine::EventCallback>
		{
			void operator () (Event event,Result result=RESULT_OK) const
			{
				if (function)
					function( userdata, event, result );
			}
		};
	}
}

#if NST_MSVC >= 1200 || NST_ICC >= 810
#pragma warning( pop )
#endif

#endif
