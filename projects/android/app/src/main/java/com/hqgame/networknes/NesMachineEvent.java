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

package com.hqgame.networknes;

/**
 * Created by le on 3/6/2016.
 */
public enum NesMachineEvent {
    /**
     * A new image has been loaded into the system.
     */
    EVENT_LOAD,
    /**
     * Connection to remote side established or failed with error.
     */
    EVENT_LOAD_REMOTE,
    /**
     * Remote controller enabled, result value is controller index
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
     * Connection to remote side finished successfully.
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
	* Message received from remote side. result value is address of message's string
	*/
    EVENT_REMOTE_MESSAGE,
    /*
    * Sent message received by remote side. result value is id of message
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
    EVENT_MODE_PAL,
}
