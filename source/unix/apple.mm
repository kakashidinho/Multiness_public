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

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include "apple.h"


//override default function in main.cpp
bool nst_choose_rom_dialog(std::string& file) {
	return openFileDialog(file, "Open nes rom", "");
}


//override function in main.cpp
bool nst_app_additional_eventHandler_iterate() {
	bool hasEvent = false;
	NSEvent *event;
	while ((event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES]) != nil)
	{
		[NSApp sendEvent: event];
		
		hasEvent = true;
	}
	
	return hasEvent;
}

std::string getResourcePathApple() {
	auto nsPath = [[NSBundle mainBundle] resourcePath];
	if (nsPath != nil)
		return [nsPath UTF8String];
	return ".";
}

bool openFileDialog(std::string& fileSelected, const char* title, const char* filter)
{
	char* filter_copy = strdup(filter);
	if (!filter_copy)
		return false;
	NSMutableArray* nsFilterTypes = [NSMutableArray array];
	char* tokCtxt;
	char* types = strtok_r(filter_copy, ", ", &tokCtxt);
	while (types != NULL)
	{
		NSString* filterType = [NSString stringWithUTF8String:types];
		NSRange dot = [filterType rangeOfString:@"." options:NSBackwardsSearch];
		NSString* ext = [filterType substringFromIndex:dot.location + 1];
		
		[nsFilterTypes addObject:ext];
		types = strtok_r(NULL, ", ", &tokCtxt);
	}
	
	free(filter_copy);
	
	NSOpenPanel* panel = [NSOpenPanel openPanel];
	
	panel.title = [NSString stringWithUTF8String:title];
	
	panel.canChooseFiles = YES;
	if ([nsFilterTypes count])
		panel.allowedFileTypes = nsFilterTypes;
	
	NSUInteger res = [panel runModal];
	
	if ( res == NSModalResponseOK && [panel.URLs count] > 0) {
		NSURL* url = [panel.URLs objectAtIndex:0];
		
		fileSelected = [url.path UTF8String];
		
		return true;
	}
	return false;
}