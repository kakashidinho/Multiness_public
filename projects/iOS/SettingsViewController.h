//
//  SettingsViewController.h
//  nestopia
//
//  Created by Le Hoang Quyen on 23/5/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SettingsViewController : UIViewController

@property (strong, nonatomic) IBOutlet UISlider *dpadSizeSlider;
@property (strong, nonatomic) IBOutlet UISwitch *voiceChatCheckBox;
@property (strong, nonatomic) IBOutlet UITextField *orientationPickerField;

@end
