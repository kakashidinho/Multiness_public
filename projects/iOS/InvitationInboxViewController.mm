//
//  InvitationInboxViewController.m
//  nestopia
//
//  Created by Le Hoang Quyen on 5/30/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#import "InvitationInboxViewController.h"

#include "Utils.h"
#include "Settings.h"

#include "../../source/remote_control/ConnectionHandlerRakNet.hpp"

#import "../../../third-party/MBProgressHUD/MBProgressHUD.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit.h>

/*------  RequestCellButton -----*/
@interface RequestCellButton : UIButton

@property (weak, atomic) UITableViewCell* parentCell;

@end

@implementation RequestCellButton

@end

/*------  RequestCell  ----*/
@interface RequestCell : UITableViewCell
@property (weak, nonatomic) IBOutlet RequestCellButton *rejectBtn;
@property (weak, nonatomic) IBOutlet RequestCellButton *acceptBtn;
@property (weak, nonatomic) IBOutlet UILabel *ownerLabel;

@property (weak) NSDictionary* requestInfo;

@end

@implementation RequestCell

@end

/*------  InvitationInboxViewController ---*/
@interface InvitationInboxViewController()

@property (strong, atomic) NSMutableArray* sortedInvitationRequests;
@property (strong, atomic) NSMutableSet *invitationOwnerIds;
@property (strong, atomic) NSMutableArray* invitations;

@property std::shared_ptr<Nes::Remote::ConnectionHandlerRakNetClient> connectivityTester;

@end

@implementation InvitationInboxViewController

- (void)viewDidLoad {
	[super viewDidLoad];
	
	_invitationListBox.delegate = self;
	_invitationListBox.dataSource = self;
	
	//actual data
	_invitations = [NSMutableArray array];
	_invitationOwnerIds = [NSMutableSet set];
	
	//display progress dialog
	displayProgressMsg(self.view, @"Loading ...", self, @selector(cancelAction:));
	
	//fetch data by using Graph API
	FBSDKGraphRequest *request = [[FBSDKGraphRequest alloc]
								  initWithGraphPath:@"/me/apprequests"
								  parameters:@{@"fields": @"data,paging,from,message,created_time"}
								  HTTPMethod:@"GET"];
	[request startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection,
										  id result,
										  NSError *error) {
		// Handle the result
		if (error)
		{
			displayErrorMsg(self, @"Error", [error localizedDescription], ^{
				[self cancelAction:nil];
			});
		}
		else {
			//TODO: paging
			NSArray* requests = [result objectForKey:@"data"];
			if (requests) {
				NSDate *currentTime = [NSDate date];
				
				_sortedInvitationRequests = [NSMutableArray array];
				
				//decode creation time
				NSDateFormatter *df = [[NSDateFormatter alloc] init];
				//2010-12-01T21:35:43+0000
				[df setDateFormat:@"yyyy-MM-dd'T'HH:mm:ssZZZZ"];
				
				for (NSDictionary* request in requests) {
					NSString* time_str = [request objectForKey:@"created_time"];
					
					NSDate *nstime = time_str != nil ? [df dateFromString:time_str] : currentTime;
					
					//discard out of date request (more than two days old)
					auto timeInvervalSinceThen = [currentTime timeIntervalSinceDate:nstime];
					
					if (timeInvervalSinceThen < 60 * 60 * 48)
					{
						NSMutableDictionary* copiedRequest = [NSMutableDictionary dictionaryWithDictionary:request];
						[copiedRequest setObject:nstime forKey:@"created_time_nsdate"];
					
						[_sortedInvitationRequests addObject:copiedRequest];
					}//if (timeInvervalSinceThen < 60 * 60 * 48)
					else {
						//delete this request
						[self deleteRequest:request];
					}//if (timeInvervalSinceThen < 60 * 60 * 48)
				}//for (NSDictionary* request in requests)
				
				//sort the requests based on creation time
				[_sortedInvitationRequests sortUsingComparator:^NSComparisonResult(NSDictionary* a, NSDictionary* b) {
					
					NSDate *first = [a objectForKey:@"created_time_nsdate"];
					NSDate *second = [b objectForKey:@"created_time_nsdate"];
					
					auto re = [first compare:second];
					return re;
				}];
				//insert to data, removed out-of-date requests from the same person turn by turn
				[self iterateRequestValidation];
			}//if (requests)
		}
	}];
}

- (void) deleteRequest: (NSDictionary*)request
{
	NSString* request_id = [request objectForKey:@"id"];
	
	FBSDKGraphRequest *graph_request = [[FBSDKGraphRequest alloc]
								  initWithGraphPath:[NSString stringWithFormat:@"/%@", request_id]
								  parameters:nil
								  HTTPMethod:@"DELETE"];
	[graph_request startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection,
											id result,
											NSError *error) {
		// Handle the result
	}];
}

- (void) iterateRequestValidation {
	if ([_sortedInvitationRequests count])
	{
		auto last_idx = [_sortedInvitationRequests count] - 1;
		NSDictionary* request = [_sortedInvitationRequests objectAtIndex: last_idx];
		[_sortedInvitationRequests removeObjectAtIndex:last_idx];
		
		
		NSDictionary* from = [request objectForKey:@"from"];
		NSString* guid = [request objectForKey:@"data"];
		NSString* fromId = [from objectForKey:@"id"];
		
		if (![_invitationOwnerIds containsObject:fromId]) {
			[_invitationOwnerIds addObject:fromId];
			[_invitations addObject:request];
			
			//process next request
			dispatch_async(dispatch_get_main_queue(), ^{
				[self iterateRequestValidation];
			});
			
		}//if ([_invitationOwnerIds objectForKey:fromId] == nil)
		else {
			//this request is out of date
			//delete it
			[self deleteRequest:request];
			
			//process next request
			dispatch_async(dispatch_get_main_queue(), ^{
				[self iterateRequestValidation];
			});
		}
	} else {
		//all requests has been processed
		[_invitationListBox reloadData];
		
		//dismiss progress dialog
		dismissProgressMsg(self.view);
	}
}

#pragma mark IBAction
- (IBAction)cancelAction:(id)sender {
	if (_connectivityTester)
	{
		_connectivityTester->stop();
		_connectivityTester = nullptr;
	}
	
	dismissProgressMsg(self.view);
	
	[self dismissViewControllerAnimated:YES completion:nil];
}

- (IBAction)acceptAction:(id)sender {
	RequestCellButton* btn = (RequestCellButton*)sender;
	RequestCell* cell = (RequestCell*)btn.parentCell;
	
	NSDictionary* request = cell.requestInfo;
	
	NSString* data = [request objectForKey:@"data"];
	
	[self deleteRequest:request];
	
	//close view controller
	[self dismissViewControllerAnimated:YES completion:^ {
		if (_delegate)
			[_delegate onInvitationAccepted:data];
	}];
}

- (IBAction)rejectAction:(id)sender {
	RequestCellButton* btn = (RequestCellButton*)sender;
	RequestCell* cell = (RequestCell*)btn.parentCell;
	
	NSDictionary* request = cell.requestInfo;
	
	[self deleteRequest:request];
}

#pragma  mark TableView delegate
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
	return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
	return [_invitations count];
}


- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
	return @"Invitations";
}


- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
	static NSString *MyIdentifier = @"RequestCell";
	RequestCell *cell = [tableView dequeueReusableCellWithIdentifier:MyIdentifier];
	
	NSDictionary* request = [_invitations objectAtIndex:indexPath.row];
	NSDictionary* requestOwner = [request objectForKey:@"from"];
	
	NSString* from = [requestOwner objectForKey:@"name"];
	NSString* message = [request objectForKey:@"message"];
	
	cell.requestInfo = request;
	cell.ownerLabel.text = from;
	cell.acceptBtn.parentCell = cell.rejectBtn.parentCell = cell;
	
	return cell;
}

@end
