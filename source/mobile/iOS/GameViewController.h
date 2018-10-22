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

#ifndef GameViewController_h
#define GameViewController_h

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

#import <FBSDKShareKit/FBSDKShareKit.h>

typedef enum REMOTE_CONTROL_MODE {
	NO_REMOTE_CONTROL,
	HOST_LAN_REMOTE_CONTROL,
	CONNECT_LAN_REMOTE_CONTROL,
	HOST_INTERNET_REMOTE_CONTROL,
	CONNECT_INTERNET_REMOTE_CONTROL,
	QUICK_CONNECT_INTERNET_REMOTE_CONTROL,
} REMOTE_CONTROL_MODE;

@interface GameViewController : UIViewController<GLKViewDelegate, FBSDKGameRequestDialogDelegate>

@property (assign) REMOTE_CONTROL_MODE remoteCtlMode;
@property (strong) NSString* hostIP;//in lan mode, this is ip of server, in internet mode, this is GUID + key of server
@property (assign) int hostPort;
@property (strong) NSDictionary* clientInfo;
@property (strong) NSString* gamePath;//should be nil when remoteCtlMode = CONNECT_LAN_REMOTE_CONTROL
@property (strong, atomic) void (^didDidmissCallback) ();

@property (strong, nonatomic) IBOutlet GLKView *glview;

@end


#endif /* GameViewController_h */
