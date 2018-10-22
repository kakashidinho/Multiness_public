//
//  InvitationInboxViewController.h
//  nestopia
//
//  Created by Le Hoang Quyen on 5/30/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#import <UIKit/UIKit.h>

@protocol InvitationInboxDelegate <NSObject>

- (void)onInvitationAccepted: (NSString*) invitationData;

@end

@interface InvitationInboxViewController : UIViewController<UITableViewDelegate, UITableViewDataSource>

@property (weak, nonatomic) IBOutlet UITableView *invitationListBox;

@property (assign) id<InvitationInboxDelegate> delegate;

@end
