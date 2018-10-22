//
//  SettingsViewController.m
//  nestopia
//
//  Created by Le Hoang Quyen on 23/5/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#import "SettingsViewController.h"
#import "Settings.h"
#import "DownPicker.h"

static const UIInterfaceOrientationMask g_supportedOrientation[] = {
	UIInterfaceOrientationMaskLandscape,
	UIInterfaceOrientationMaskLandscapeLeft,
	UIInterfaceOrientationMaskLandscapeRight,
	UIInterfaceOrientationMaskPortrait,
	UIInterfaceOrientationMaskAll};

static NSString* g_supportedOrientationName[] = {
	@"landscape",
	@"landscape left",
	@"landscape right",
	@"portrait",
	@"auto",
};

@interface SettingsViewController ()

@property (strong, atomic) DownPicker* orientationPicker;

@end

@implementation SettingsViewController

- (void)viewDidLoad {
	[super viewDidLoad];
	
	//read saved settings
	BOOL voiceChat = [Settings sharedInstance].voiceChatEnabled;
	float dpadScale = [Settings sharedInstance].dpadScale;
	UIInterfaceOrientationMask ori = [Settings sharedInstance].orientation;
	
	_voiceChatCheckBox.on = voiceChat;
	_dpadSizeSlider.value = dpadScale;
	
	//setup orientation picker
	NSMutableArray* orientationNameArray = [NSMutableArray array];
	size_t selectedOrientationIdx = 0;
	for (size_t i = 0; i < sizeof(g_supportedOrientation) / sizeof(g_supportedOrientation[0]); ++i)
	{
		if (g_supportedOrientation[i] == ori)
		{
			selectedOrientationIdx = i;
		}
	
		[orientationNameArray addObject:g_supportedOrientationName[i]];
	}
	
	_orientationPicker = [[DownPicker alloc] initWithTextField:_orientationPickerField withData:orientationNameArray];
	_orientationPicker.selectedIndex = selectedOrientationIdx;
}

- (void)didReceiveMemoryWarning {
	[super didReceiveMemoryWarning];
	// Dispose of any resources that can be recreated.
}

- (IBAction)backAction:(id)sender {
	//save settings
	[Settings sharedInstance].voiceChatEnabled = _voiceChatCheckBox.on;
	[Settings sharedInstance].dpadScale = _dpadSizeSlider.value;
	NSInteger selectedOrientationIdx = _orientationPicker.selectedIndex;
	[Settings sharedInstance].orientation = g_supportedOrientation[selectedOrientationIdx];
	
	//exit
	[self dismissViewControllerAnimated:YES completion:nil];
}

@end
