//
//  AppDelegate.m
//  nestopiaIOS
//
//  Created by Le Hoang Quyen on 29/2/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#import "AppDelegate.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>

#include "../../third-party/RemoteController/ConnectionHandler.h"

#if defined DEBUG || defined _DEBUG
class ServerDiscoveryDebug: public HQRemote::SocketServerDiscoverClientHandler::DiscoveryDelegate {
public:
	virtual void onNewServerDiscovered(HQRemote::SocketServerDiscoverClientHandler* handler, uint64_t request_id, const char* addr, int reliablePort, int unreliablePort, const char* desc) override{
		NSLog(@"Discovered %s %s:(tcp=%d, udp=%d)", desc, addr, reliablePort, unreliablePort);
	}
};

static ServerDiscoveryDebug gServerDiscoveryDebug;
static HQRemote::SocketServerDiscoverClientHandler gServerDiscoveryHandler(&gServerDiscoveryDebug);
#endif//#if defined DEBUG || defined _DEBUG

@interface AppDelegate ()

@end

@implementation AppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
	//register for receiving push notification
	if ([[[ UIDevice currentDevice] systemVersion] floatValue] >= 8.0)
	{
		[application registerUserNotificationSettings:[UIUserNotificationSettings settingsForTypes:(UIUserNotificationTypeSound | UIUserNotificationTypeAlert | UIUserNotificationTypeBadge) categories:nil]];
		
		[application registerForRemoteNotifications];
	}
	else
	{
		[application registerForRemoteNotificationTypes:(UIUserNotificationTypeBadge | UIUserNotificationTypeSound | UIUserNotificationTypeAlert)];
	}
	
	[[FBSDKApplicationDelegate sharedInstance] application:application
							 didFinishLaunchingWithOptions:launchOptions];
	
#if defined DEBUG || defined _DEBUG
	gServerDiscoveryHandler.start();
	gServerDiscoveryHandler.findOtherServers(1);
#endif
	
	return YES;
}

//APNS token handler for GPG
- (void)application:(UIApplication *)application
didRegisterForRemoteNotificationsWithDeviceToken
				   :(NSData *)deviceToken {
	NSLog(@"Got deviceToken from APNS! %@", deviceToken);
}

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(NSDictionary *)userInfo {
	
}

//
//openURL handler to catch openURL invocation from G+ application.
//Also you need to register URL type and add GPGApplicationID property.
//For more details, please refer https://developers.google.com/games/services/ios/quickstart
//
- (BOOL)application:(UIApplication *)application
			openURL:(NSURL *)url
  sourceApplication:(NSString *)sourceApplication
		 annotation:(id)annotation {
	
	return [[FBSDKApplicationDelegate sharedInstance] application:application
														  openURL:url
												sourceApplication:sourceApplication
													   annotation:annotation];
}

- (void)applicationWillResignActive:(UIApplication *)application {
	// Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
	// Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
	// Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
	// If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
	// Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
	
	[FBSDKAppEvents activateApp];	
	[FBSDKSettings setLoggingBehavior: [NSSet setWithObject:FBSDKLoggingBehaviorGraphAPIDebugInfo]];
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
	// Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}

- (void)applicationWillTerminate:(UIApplication *)application {
	// Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

@end
