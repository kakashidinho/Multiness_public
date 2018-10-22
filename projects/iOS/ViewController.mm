//
//  ViewController.m
//  nestopiaIOS
//
//  Created by Le Hoang Quyen on 29/2/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#import <UIKit/UIKit.h>

#import "ViewController.h"
#import "GameViewController.h"
#import "Utils.h"

#import "../../third-party/ios_file_browser/source/PXRFileBrowser.h"

#define DEFAULT_REMOTE_PORT 23458

static NSString* gIPPreferenceKey = @"IP";

typedef void (^rom_selection_handler)(NSString* path);

@interface ViewController ()

@property (strong, nonatomic) IBOutlet FBSDKLoginButton  *signInButton;
@property (strong, atomic) rom_selection_handler romSelectionHandler;
@property (strong) GameViewController* gameViewController;

@property (strong, atomic) id fbProfileDetails;

@end

@implementation ViewController

- (void)viewDidLoad {
	[super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
	
	//initialize Facebook stuffs
	_signInButton.readPermissions =
	@[@"public_profile", @"email", @"user_friends"];
	
	_signInButton.delegate = self;
	
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(onLogInStateChanged:) name:FBSDKAccessTokenDidChangeNotification object:nil];
	
	[self checkAutoLoginOnStart];
}

- (void)didReceiveMemoryWarning {
	[super didReceiveMemoryWarning];
	// Dispose of any resources that can be recreated.
}

- (GameViewController*) createGameViewController {
	return  [self.storyboard instantiateViewControllerWithIdentifier:@"gameScreen"];
}

- (IBAction)signInOutAction:(id)sender {
	
}
- (IBAction)settingsAction:(id)sender {
	UIViewController* view = [self.storyboard instantiateViewControllerWithIdentifier:@"settingsScreen"];
	[self presentViewController:view animated:YES completion:nil];
}

- (IBAction)testAction:(id)sender {
	[self selectGameWithCompletion:^(NSString* path) {
		[self testLanServer:path];
	}];
}

- (IBAction)testClientAction:(id)sender {
	__block UITextField* ipTextField;
	UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Remote control"
																   message:@"Connect to host"
															preferredStyle:UIAlertControllerStyleAlert];
	[alert addAction:[UIAlertAction actionWithTitle:@"OK"
											  style:UIAlertActionStyleDefault
											handler:^(UIAlertAction *action){
												NSString* ip = ipTextField.text;
												[self testLanClientWithHostIP:ip];
											}]];
	[alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
											  style:UIAlertActionStyleDefault
											handler:nil]];
	[alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
		textField.placeholder = @"IP:";
		ipTextField = textField;
		
		//read any saved ip
		NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
		if (preferences == nil)
		{
			return;
		}
		
		NSString* saved_ip = [preferences objectForKey:gIPPreferenceKey];
		if (saved_ip != nil)
			textField.text = saved_ip;
	}];
	
	//show ip input dialog
	[self presentViewController:alert animated:YES completion:nil];
}

- (IBAction)testGPGHostAction:(id)sender {
	[self selectGameWithCompletion:^(NSString* path) {
		[self testGPGServer:path];
	}];
}

- (IBAction)testGPGClientAction:(id)sender {
	if ([self showErrorIfNotSignedIn])
		return;

	InvitationInboxViewController* view = [self.storyboard instantiateViewControllerWithIdentifier:@"invitationInbox"];
	
	view.delegate = self;
	
	[self presentViewController:view animated:YES completion:nil];
}

- (IBAction)testGPGQuickAction:(id)sender {
	if ([self showErrorIfNotSignedIn])
		return;
	
	//TODO
	GameViewController* gameView = [self createGameViewController];
	
	gameView.remoteCtlMode = QUICK_CONNECT_INTERNET_REMOTE_CONTROL;
	gameView.clientInfo = _fbProfileDetails;
	
	[self presentViewController:gameView animated:YES completion:nil];
}

//Internet invitation accepted
- (void)onInvitationAccepted: (NSString*) invitationData
{
	GameViewController* gameView = [self createGameViewController];
	
	gameView.remoteCtlMode = CONNECT_INTERNET_REMOTE_CONTROL;
	gameView.clientInfo = _fbProfileDetails;
	
	//in internet mode, hostIP is GUID of server
	gameView.hostIP = invitationData;
	
	[self presentViewController:gameView animated:YES completion:nil];
}

- (BOOL) showErrorIfNotSignedIn {
	if (_fbProfileDetails == nil)
	{
		UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Error"
																	   message:@"You need to sign in"
																preferredStyle:UIAlertControllerStyleAlert];
		[alert addAction:[UIAlertAction actionWithTitle:@"OK"
												  style:UIAlertActionStyleDefault
												handler:nil]];
		[self presentViewController:alert animated:YES completion:nil];
		return YES;
	}
	
	return NO;
}

- (void)testLanServer: (NSString*)romPath {
	UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Remote control"
																   message:[NSString stringWithFormat:@"Hosting a new game\n Your ip is %@", getLocalIPAddress()]
															preferredStyle:UIAlertControllerStyleAlert];
	[alert addAction:[UIAlertAction actionWithTitle:@"OK"
											  style:UIAlertActionStyleDefault
											handler:^(UIAlertAction* action) {
												GameViewController* gameView = [self createGameViewController];
												
												gameView.remoteCtlMode = HOST_LAN_REMOTE_CONTROL;
												gameView.hostPort = DEFAULT_REMOTE_PORT;
												gameView.clientInfo = @{@"name": getLocalIPAddress()};
												gameView.gamePath = romPath;
												
												[self presentViewController:gameView animated:YES completion:nil];
											}]];
	
	dispatch_async(dispatch_get_main_queue(), ^{
		[self presentViewController:alert animated:YES completion:nil];
	});
}

- (void)testLanClientWithHostIP: (NSString*)ip {
	if (ip == nil)
	{
		UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Error"
																	   message:@"Invalid ip"
																preferredStyle:UIAlertControllerStyleAlert];
		[alert addAction:[UIAlertAction actionWithTitle:@"OK"
												  style:UIAlertActionStyleDefault
												handler:nil]];
		[self presentViewController:alert animated:YES completion:nil];
		return;
	}
	
	//save to preferences
	NSUserDefaults *preferences = [NSUserDefaults standardUserDefaults];
	if (preferences != nil)
	{
		[preferences setObject:ip forKey:gIPPreferenceKey];
		
		[preferences synchronize];
	}
	
	//start game
	GameViewController* gameView = [self createGameViewController];
	
	gameView.remoteCtlMode = CONNECT_LAN_REMOTE_CONTROL;
	gameView.hostIP = ip;
	gameView.hostPort = DEFAULT_REMOTE_PORT;
	gameView.clientInfo = @{@"name": getLocalIPAddress()};
	gameView.gamePath = nil;
	
	[self presentViewController:gameView animated:YES completion:nil];
}

- (void)testGPGServer: (NSString*)romPath {
	if ([self showErrorIfNotSignedIn])
		return;
	
	GameViewController* gameView = [self createGameViewController];
	
	gameView.remoteCtlMode = HOST_INTERNET_REMOTE_CONTROL;
	gameView.clientInfo = _fbProfileDetails;
	gameView.gamePath = romPath;
	
	dispatch_async(dispatch_get_main_queue(), ^{
		[self presentViewController:gameView animated:YES completion:nil];
	});
}

- (void) selectGameWithCompletion: (rom_selection_handler) handler {
	if (_romSelectionHandler != nil)
		return;//we already have another dialog still opening
	_romSelectionHandler = handler;
	
	PXRFileBrowser* fileBrowser = [[PXRFileBrowser alloc] init];
	fileBrowser.delegate = self;
	fileBrowser.autoLoadPickedFile = NO;
	
	[fileBrowser browseForFileWithType:@"*" inDirectory:[[NSBundle mainBundle] pathForResource:@"testGames" ofType:nil]];
	
	[self presentViewController:fileBrowser animated:YES completion:nil];
}

//override UIViewController
- (void)presentViewController:(UIViewController *)viewControllerToPresent
					 animated:(BOOL)flag
				   completion:(void (^)(void))completion {
	if ([viewControllerToPresent isKindOfClass:[GameViewController class]]) {
		_gameViewController = (GameViewController*)viewControllerToPresent;
		_gameViewController.didDidmissCallback = ^{
			_gameViewController = nil;
		};
	}
	
	[super presentViewController:viewControllerToPresent animated:flag completion:completion];
}

#pragma mark -
#pragma mark Facebook

-(void)fetchProfileDetailsWithCallback: (void (^)(id result))callback
{
	FBSDKGraphRequest *request = [[FBSDKGraphRequest alloc] initWithGraphPath:@"me" parameters:@{@"fields": @"name,id"}];
	[request startWithCompletionHandler:^(FBSDKGraphRequestConnection *connection, id result, NSError *error) {
		if (!error && result) {
			callback(result);
		}
		else {
			callback(nil);
			
			displayErrorMsg(self, @"Error", [error localizedDescription], nil);
		}
	}];
}

- (void) checkAutoLoginOnStart {
	if ([FBSDKAccessToken currentAccessToken] != nil)
	{
		[self onLogInStateChanged:nil];
	}
}

- (void) onLogInStateChanged: (id)sender {
	if ([FBSDKAccessToken currentAccessToken] != nil) {
		displayProgressMsg(self.view, @"Loading Facebook profile ...", nil, nil);
		
		[self fetchProfileDetailsWithCallback:^(id result){
			_fbProfileDetails = result;
			
			dismissProgressMsg(self.view);
		}];
	}
	else {
		_fbProfileDetails = nil;
	}
}

/*!
 @abstract Sent to the delegate when the button was used to login.
 @param loginButton the sender
 @param result The results of the login
 @param error The error (if any) from the login
 */
- (void)  loginButton:(FBSDKLoginButton *)loginButton
didCompleteWithResult:(FBSDKLoginManagerLoginResult *)result
				error:(NSError *)error {
}

/*!
 @abstract Sent to the delegate when the button was used to logout.
 @param loginButton The button that was clicked.
 */
- (void)loginButtonDidLogOut:(FBSDKLoginButton *)loginButton {
}

/*!
 @abstract Sent to the delegate when the button is about to login.
 @param loginButton the sender
 @return YES if the login should be allowed to proceed, NO otherwise
 */
- (BOOL) loginButtonWillLogin:(FBSDKLoginButton *)loginButton {
	
	return YES;
}

#pragma mark -
#pragma mark filebrowser callbacks
- (void)fileBrowserFinishedPickingFile:(NSData*)file withName:(NSString*)fileName {
	if (_romSelectionHandler != nil)
		_romSelectionHandler(fileName);
	_romSelectionHandler = nil;
}

- (void)fileBrowserCanceledPickingFile {
	_romSelectionHandler = nil;
}
- (void)fileBrowserFinishedSavingFileNamed:(NSString*)fileName {
	
}
- (void)fileBrowserCanceledSavingFile:(NSData*)file {
	
}

@end
