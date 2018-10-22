//
//  ViewController.h
//  nestopiaIOS
//
//  Created by Le Hoang Quyen on 29/2/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#import <UIKit/UIKit.h>

#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit.h>

#import "InvitationInboxViewController.h"

@interface ViewController : UIViewController<InvitationInboxDelegate, FBSDKLoginButtonDelegate>


@end

