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

#import "Settings.h"

static NSString* VOICE_SETTINGS_KEY = @"voice";
static NSString* DPAD_SCALE_SETTINGS_KEY = @"dpad_scale";
static NSString* ORIENTATION_SETTINGS_KEY = @"orientation";

@implementation Settings

@synthesize voiceChatEnabled = _voiceChatEnabled;
@synthesize dpadScale = _dpadScale;
@synthesize orientation = _orientation;

+ (Settings*) sharedInstance {
	static Settings* s_instance = nil;
	if (!s_instance)
		s_instance = [[Settings alloc] init];
	
	return s_instance;
}

- (id) init {
	id re;
	
	if ((re = [super init]) != nil) {
               // TODO: use your server, this server could be closed at anytime
		_NAT_SERVER = @"125.212.218.120";
		_NAT_SERVER_PORT = 61111;
		
		_dpadScale = 1.f;
		_voiceChatEnabled = YES;
		_orientation = UIInterfaceOrientationMaskLandscape;
		
		//read settings
		NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
		if (preferences != nil)
		{
			if ([preferences objectForKey:VOICE_SETTINGS_KEY] != nil)
				_voiceChatEnabled = [preferences boolForKey:VOICE_SETTINGS_KEY];
			if ([preferences objectForKey:DPAD_SCALE_SETTINGS_KEY] != nil)
				_dpadScale = [preferences floatForKey:DPAD_SCALE_SETTINGS_KEY];
			if ([preferences objectForKey:ORIENTATION_SETTINGS_KEY])
				_orientation = [preferences integerForKey:ORIENTATION_SETTINGS_KEY];
		}
	}
	
	return re;
}

- (BOOL) voiceChatEnabled {
	return _voiceChatEnabled;
}

- (void) setVoiceChatEnabled:(BOOL)voiceChatEnabled {
	_voiceChatEnabled = voiceChatEnabled;
	
	//save settings
	NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
	if (preferences)
	{
		[preferences setBool:voiceChatEnabled forKey:VOICE_SETTINGS_KEY];
		[preferences synchronize];
	}
}

- (float) dpadScale {
	return _dpadScale;
}

- (void) setDpadScale:(float)scale {
	_dpadScale = scale;
	
	//save settings
	NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
	if (preferences)
	{
		[preferences setFloat:scale forKey:DPAD_SCALE_SETTINGS_KEY];
		[preferences synchronize];
	}
}

- (UIInterfaceOrientationMask) orientation {
	return _orientation;
}

- (void) setOrientation:(UIInterfaceOrientationMask)orientation
{
	_orientation = orientation;
	
	//save settings
	NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
	if (preferences)
	{
		[preferences setInteger:orientation forKey:ORIENTATION_SETTINGS_KEY];
		[preferences synchronize];
	}
}

@end
