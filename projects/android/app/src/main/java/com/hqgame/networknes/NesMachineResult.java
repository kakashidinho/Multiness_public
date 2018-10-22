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
public class NesMachineResult {
    public static boolean failed(int result)
    {
        return result < RESULT_OK;
    }

    public static boolean succeeded(int result)
    {
        return result >= RESULT_OK;
    }

    /**
     * Buffer is too big
     */
    public static final int RESULT_ERR_BUFFER_TOO_BIG = -15;
    /**
     * Failed to establish connection
     */
    public static final int RESULT_ERR_CONNECTION = -14;
    /**
     * NTSC/PAL region mismatch.
     */
    public static final int RESULT_ERR_WRONG_MODE = -13;
    /**
     * Missing FDS BIOS.
     */
    public static final int RESULT_ERR_MISSING_BIOS = -12;
    /**
     * Unsupported or malformed mapper.
     */
    public static final int RESULT_ERR_UNSUPPORTED_MAPPER = -11;
    /**
     * Vs DualSystem is unsupported.
     */
    public static final int RESULT_ERR_UNSUPPORTED_VSSYSTEM = -10;
    /**
     * File format version is no longer supported.
     */
    public static final int RESULT_ERR_UNSUPPORTED_FILE_VERSION = -9;
    /**
     * Unsupported operation.
     */
    public static final int RESULT_ERR_UNSUPPORTED = -8;
    /**
     * Invalid CRC checksum.
     */
    public static final int RESULT_ERR_INVALID_CRC = -7;
    /**
     * Corrupt file.
     */
    public static final int RESULT_ERR_CORRUPT_FILE = -6;
    /**
     * Invalid file.
     */
    public static final int RESULT_ERR_INVALID_FILE = -5;
    /**
     * Invalid parameter(s).
     */
    public static final int RESULT_ERR_INVALID_PARAM = -4;
    /**
     * System not ready.
     */
    public static final int RESULT_ERR_NOT_READY = -3;
    /**
     * Out of memory.
     */
    public static final int RESULT_ERR_OUT_OF_MEMORY = -2;
    /**
     * Generic error.
     */
    public static final int RESULT_ERR_GENERIC = -1;
    /**
     * Success.
     */
    public static final int RESULT_OK = 0;
    /**
     * Success but operation had no effect.
     */
    public static final int RESULT_NOP = 1;
    /**
     * Success but image dump may be bad.
     */
    public static final int RESULT_WARN_BAD_DUMP = 2;
    /**
     * Success but PRG-ROM may be bad.
     */
    public static final int RESULT_WARN_BAD_PROM = 3;
    /**
     * Success but CHR-ROM may be bad.
     */
    public static final int RESULT_WARN_BAD_CROM = 4;
    /**
     * Success but file header may have incorrect data.
     */
    public static final int RESULT_WARN_BAD_FILE_HEADER = 5;
    /**
     * Success but save data has been lost.
     */
    public static final int RESULT_WARN_SAVEDATA_LOST = 6;
    /**
     * Success but data may have been replaced.
     */
    public static final int RESULT_WARN_DATA_REPLACED = 8;
}
