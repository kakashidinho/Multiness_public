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

#import "Utils.h"

#include <ifaddrs.h>
#include <arpa/inet.h>

#import "../../../third-party/MBProgressHUD/MBProgressHUD.h"

NSString *getLocalIPAddress() {
	
	NSString *address = @"unknown";
	struct ifaddrs *interfaces = NULL;
	struct ifaddrs *temp_addr = NULL;
	int success = 0;
	// retrieve the current interfaces - returns 0 on success
	success = getifaddrs(&interfaces);
	if (success == 0) {
		// Loop through linked list of interfaces
		temp_addr = interfaces;
		while(temp_addr != NULL) {
			if(temp_addr->ifa_addr->sa_family == AF_INET) {
				// Check if interface is en0 which is the wifi connection on the iPhone
				if([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:@"en0"]) {
					// Get NSString from C String
					address = [NSString stringWithUTF8String:inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr)];
					
				}
				
			}
			
			temp_addr = temp_addr->ifa_next;
		}
	}
	// Free memory
	freeifaddrs(interfaces);
	return address;
	
}

NSString *getWritableDirectory()
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	NSString *basePath = paths.firstObject;
	return basePath;
}

void displayErrorMsg(UIViewController* currentVC, NSString* title, NSString* msg, void (^ closeHandler)()) {
	displayErrorMsgWithRetryBtn(currentVC, title, msg, nil, closeHandler);
}

void displayErrorMsgWithRetryBtn(UIViewController* currentVC, NSString* title, NSString* msg, void (^ retryHandler)(), void (^ closeHandler)()) {
	UIAlertController *alert =
	[UIAlertController alertControllerWithTitle:title
										message:msg
								 preferredStyle:UIAlertControllerStyleAlert];
	
	if (retryHandler != nil) {
		UIAlertAction *retryAction = [UIAlertAction actionWithTitle:@"Retry"
														 style:UIAlertActionStyleDefault
													   handler:^(UIAlertAction *action) {
														   retryHandler();
													   }];
		[alert addAction:retryAction];
	}
	
	UIAlertAction *action = [UIAlertAction actionWithTitle:@"OK"
													 style:UIAlertActionStyleDefault
												   handler:^(UIAlertAction *action) {
													   if (closeHandler)
														   closeHandler();
												   }];
	[alert addAction:action];
	
	[currentVC presentViewController:alert animated:YES completion:nil];
}

void displayProgressMsg(UIView* rootView, NSString* msg, id closeActionTarget, SEL closeActionSelector) {
	
	MBProgressHUD* dialog = [MBProgressHUD showHUDAddedTo:rootView animated:YES];
	[dialog.label setText:msg];
	[dialog.button setTitle:@"Cancel" forState:UIControlStateNormal];
	[dialog.button addTarget:closeActionTarget action:closeActionSelector forControlEvents:UIControlEventTouchUpInside];
}

void dismissProgressMsg(UIView* rootView) {
	[MBProgressHUD hideHUDForView:rootView animated:NO];
}