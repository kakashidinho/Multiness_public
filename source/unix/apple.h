/*
 * Nestopia UE
 *
 * Copyright (C) 2016-2018 Le Hoang Quyen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef apple_h
#define apple_h

#include <string>

std::string getResourcePathApple();

bool openFileDialog(std::string& fileSelected, const char* title, const char* filter);

#define DATADIR getResourcePathApple().c_str()


#endif /* apple_h */
