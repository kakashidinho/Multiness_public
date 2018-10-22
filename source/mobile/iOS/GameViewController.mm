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

#import <Foundation/Foundation.h>

#import "GameViewController.h"
#import "Settings.h"
#import "Utils.h"
#import "AudioDriver.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>

#include <RakPeer.h>

#include "../InputGLES.hpp"
#include "../RendererGLES.hpp"
#include "../NesSystemWrapper.hpp"

#include "../../remote_control/ConnectionHandlerRakNet.hpp"

#include "../../core/api/NstApiMemoryStream.hpp"

#include "../../../third-party/RemoteController/ConnectionHandler.h"

#import "../../../third-party/iOSToast/UIView+Toast.h"
#import "../../../third-party/MBProgressHUD/MBProgressHUD.h"
#import "../../../third-party/ios_file_browser/source/PXRFileBrowser.h"

#include <HQDefaultDataStream.h>

#include <memory>
#include <fstream>
#include <sstream>

namespace Nes {
	/* -------  NesSystem ----*/
	class NesSystem: public NesSystemWrapper
	{
	public:
		NesSystem(int esVersionMajor, std::istream* dbStream)
		: Nes::NesSystemWrapper(dbStream), m_audioDriver(m_emulator), m_lastSyncTime(0)
		{
			auto renderer = std::unique_ptr<Video::GL::Renderer>(new Video::GL::Renderer(m_emulator));
			renderer->SetESVersion(esVersionMajor);
			
			m_renderer = std::move(renderer);
		}
		
		void SetESVersion(int versionMajor)
		{
			auto glRenderer = static_cast<Video::GL::Renderer*>(m_renderer.get());
			glRenderer->SetESVersion(versionMajor);
		}
		
		void EnableAudioInput(bool enable)
		{
			m_audioDriver.EnableInput(enable);
		}
		
		void ResetTimer() {
			m_lastSyncTime = 0;
		}
		
		void Execute(){
			const auto frameRate = m_audioDriver.GetDesiredFPS();
			const auto desiredRenderTime = 1.f / frameRate;
			
			/*--------get current time-----------*/
			uint64_t current = HQRemote::getTimeCheckPoint64();
			
			/*--- time synchornization ---*/
			if (m_lastSyncTime != 0)
			{
				auto dt = HQRemote::getElapsedTime64(m_lastSyncTime, current);
				if (dt < desiredRenderTime - 0.00001f)
				{
					auto timeToSleep = (desiredRenderTime - dt) * 1000;
					std::this_thread::sleep_for(std::chrono::milliseconds((size_t)timeToSleep));
				}
			}
			
			m_lastSyncTime = HQRemote::getTimeCheckPoint64();
			
			//execute
			NesSystemWrapper::Execute();
		}
	private:
		Sound::IOS::AudioDriver m_audioDriver;
		
		uint64_t m_lastSyncTime;
	};
}

static NSString* QUICK_SAVE_PREFIX = @"quicksave";
static NSString* SAVE_FILE_POSTFIX = @"ns";
static const int MAX_QUICK_SAVES = 5;

static std::unique_ptr<Nes::NesSystem> g_pSystem = nullptr;

/*-----------  GameViewController -----------*/
@interface GameViewController ()

@property NSString* lastRequestId;
@property NSArray* lastRequestRecipients;
@property BOOL loaded;
@property BOOL exiting;
@property (strong, atomic) CADisplayLink* displayLink;
@property (strong, atomic) PXRFileBrowser* fileBrowser;

@property (strong, nonatomic) IBOutlet UIButton *btnExit;
@property (strong, nonatomic) IBOutlet UIButton *btnSave;
@property (strong, nonatomic) IBOutlet UIButton *btnLoad;
@property (strong, nonatomic) IBOutlet UIButton *btnQuickSave;
@property (strong, nonatomic) IBOutlet UIButton *btnQuickLoad;

@end

@implementation GameViewController

- (void)viewDidLoad {
	[super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
	_loaded = NO;
	
	// Create an OpenGL ES context and assign it to the view loaded from storyboard
	_glview.multipleTouchEnabled = YES;
	int esVersion = 3;
	
	EAGLContext* context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
	if (context == nil)
	{
		//try again with ES2
		esVersion = 2;
		context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
	}
	
	_glview.context = context;
	_glview.delegate = self;
	
	// init NES machine
	[self initNesWithESVersion:esVersion];
	
	// Set animation frame rate
	_displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(update)];
	_displayLink.frameInterval = 1;
	[_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
	
	//register background event
	[[NSNotificationCenter defaultCenter] addObserver: self
											 selector: @selector(pause)
												 name: UIApplicationDidEnterBackgroundNotification
											   object: nil];
	
	[[NSNotificationCenter defaultCenter] addObserver: self
											 selector: @selector(resume)
												 name: UIApplicationDidBecomeActiveNotification
											   object: nil];
}

- (void)didReceiveMemoryWarning {
	[super didReceiveMemoryWarning];
	// Dispose of any resources that can be recreated.
}

- (void)viewDidAppear:(BOOL)animated {
	[super viewDidAppear:animated];
}

- (UIInterfaceOrientationMask) supportedInterfaceOrientations {
	return [Settings sharedInstance].orientation;
}

- (void)pause {
	_displayLink.paused = YES;
}

- (void)resume {
	_displayLink.paused = NO;
}

- (void)exit {
	g_pSystem->Shutdown();
	[self exitWithoutShutdownGame];
}

- (void)exitWithoutShutdownGame {
	_exiting = YES;
	
	[self clearAllGameRequests];
	
	g_pSystem->SetMachineEventCallback(nullptr);
	g_pSystem->DisableRemoteController(1);
	g_pSystem->CleanupGraphicsResources();
	[_displayLink invalidate];
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	[self dismissProgressDialog];
	[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)start {
	if (_exiting)
		return;
	
	//load game
	if (_gamePath != nil)
	{
		auto re = g_pSystem->LoadGame([_gamePath UTF8String]);
		if (NES_FAILED(re))
		{
			return;
		}
	}
	
	//setup remote connection
	bool re = true;
	bool enabledAudioInput = false;
	
	const char* endpointName = 0;
	RakNet::RakNetGUID endpointId;
	
	endpointId.g = RakNet::RakPeerInterface::Get64BitUniqueRandomNumber();//default random GUID
	
	if (_clientInfo != nil)
	{
		endpointName = [[_clientInfo objectForKey:@"name"]UTF8String];
		endpointId.FromString([[_clientInfo objectForKey:@"id"] UTF8String]);
	}
	
	switch (_remoteCtlMode) {
		case CONNECT_LAN_REMOTE_CONTROL:
			[self displayProgressDialog:@"Connecting to server"];
			re = g_pSystem->LoadRemote([_hostIP UTF8String], _hostPort, endpointName);
			enabledAudioInput = true;
			break;
		case HOST_LAN_REMOTE_CONTROL:
			re = g_pSystem->EnableRemoteController(1, _hostPort, endpointName);
			enabledAudioInput = true;
			break;
		case CONNECT_INTERNET_REMOTE_CONTROL:
		{
			[self displayProgressDialog:@"Connecting to server"];
			RakNet::RakNetGUID rakGUID(0);
			uint64_t key = 0;
			sscanf([_hostIP UTF8String], "%llu_%llu", &key, &rakGUID.g);
			auto connHandler = std::make_shared<Nes::Remote::ConnectionHandlerRakNetClient>(&endpointId,
																							[[Settings sharedInstance].NAT_SERVER UTF8String],
																							[Settings sharedInstance].NAT_SERVER_PORT,
																							rakGUID,
																							key);
			re = g_pSystem->LoadRemote(connHandler, endpointName);
			enabledAudioInput = true;
		}
			break;
		case HOST_INTERNET_REMOTE_CONTROL:
		{
			auto connHandler = std::make_shared<Nes::Remote::ConnectionHandlerRakNetServer>(&endpointId,
																							[[Settings sharedInstance].NAT_SERVER UTF8String],
																							[Settings sharedInstance].NAT_SERVER_PORT,
																							[self](Nes::Remote::ConnectionHandlerRakNet* handler)
			{
				auto hostHandler = static_cast<Nes::Remote::ConnectionHandlerRakNetServer*>(handler);
				[self inviteFriendWithGUID:hostHandler->getGUID() andKey:hostHandler->getInvitationKey()];
			});//TODO
			
			re = g_pSystem->EnableRemoteController(1, connHandler, endpointName);
			enabledAudioInput = true;
		}
			break;
		case QUICK_CONNECT_INTERNET_REMOTE_CONTROL:
		{
			[self displayProgressDialog:@"Connecting to server"];
			auto connHandler = nullptr;//TODO
			re = g_pSystem->LoadRemote(connHandler, endpointName);
			enabledAudioInput = true;
		}
			break;
		default:
			break;
	}
	
	if (!re)
	{
		[self error:@"Failed to establish remote connection"];
		return;
	}
	
	g_pSystem->EnableAudioInput(enabledAudioInput && [Settings sharedInstance].voiceChatEnabled);
	g_pSystem->ResetTimer();
	
	_loaded = YES;
}

- (void)restart {
	_loaded = NO;
}

- (void)update {
	[_glview display];
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
	if (_exiting)
		return;
	
	auto scale = [[UIScreen mainScreen] scale];
	auto x = rect.origin.x * scale;
	auto y = rect.origin.y * scale;
	auto width = (float)(rect.size.width * scale);
	auto height = (float)(rect.size.height * scale);
	
	auto resourceLoader = [self resouceLoader];
	
	if (!_loaded)
	{
		[self start];
		
		if (_loaded)
			g_pSystem->ResetView(width, height, true, resourceLoader);
	}
	
	if (_loaded && (width != g_pSystem->GetScreenWidth() || height != g_pSystem->GetScreenHeight()))
		g_pSystem->ResetView(width, height, false, resourceLoader);
	
		
		
	glViewport(x, y, width, height);
	glClear(GL_COLOR_BUFFER_BIT);
	
	g_pSystem->Execute();
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
	//TODO
}

- (void)initNesWithESVersion: (int)esVersion {
	if (!g_pSystem)
	{
		NSString* fullPath = [[NSBundle mainBundle] pathForResource: @"NstDatabase.xml"
															 ofType:nil ];
		
		std::ifstream dbStream([fullPath UTF8String]);
		
		g_pSystem = std::unique_ptr<Nes::NesSystem>(new Nes::NesSystem(esVersion, &dbStream));
	}
	
	g_pSystem->SetESVersion(esVersion);
	
	g_pSystem->SetErrorCallback([self](const char* msg){
		[self error:[NSString stringWithUTF8String:msg]];
	});
	
	g_pSystem->SetLogCallback([](const char* format, va_list args) {
		NSLogv([NSString stringWithUTF8String:format], args);
	});
	
	g_pSystem->SetMachineEventCallback([self](Nes::Api::Machine::Event event, Nes::Result value) {
		[self handleMachineEvent:event withValue:value];
	});
	
	g_pSystem->SetInput(new Nes::Input::GL::InputDPad());
	
	g_pSystem->ScaleDPad([Settings sharedInstance].dpadScale, [self resouceLoader]);
}

- (Nes::NesSystemWrapper::OpenFileCallback) resouceLoader {
	static auto resourceLoader = [](const char* file) {
		NSString* fullPath = [[NSBundle mainBundle] pathForResource: [NSString stringWithUTF8String:file]
															 ofType: nil ];
		
		return new HQSTDDataReader([fullPath UTF8String]);
	};
	
	return resourceLoader;
}

- (void)handleMachineEvent:(Nes::Api::Machine::Event) event withValue: (Nes::Result) value
{
	switch (event) {
		case Nes::Api::Machine::EVENT_REMOTE_CONNECTION_INTERNAL_ERROR:
		{
			auto message = (const char*)value;
			if (strcmp(message, "remote_cancel") != 0)
			{
				NSString* nsMsg = [NSString stringWithUTF8String:message];
				
				switch (_remoteCtlMode) {
					case CONNECT_LAN_REMOTE_CONTROL:
					case CONNECT_INTERNET_REMOTE_CONTROL:
					case QUICK_CONNECT_INTERNET_REMOTE_CONTROL:
					{
						[self error:nsMsg retryHandler:^{
							[self restart];
						}];
					}
						break;
					default:
						[self error:nsMsg fatal:YES];
						break;
				}
			}
		}
			break;
		case Nes::Api::Machine::EVENT_REMOTE_CONNECTED:
		{
			[self dismissProgressDialog];
			
			auto name = (const char*)value;
			if (name)
			{
				auto nameUTF8 = [NSString stringWithUTF8String:name];
				[self.view makeToast:[NSString stringWithFormat:@"Connected to %@", nameUTF8]
							duration:5.
							position:CSToastPositionTop];
			}
		}
			break;
		case Nes::Api::Machine::EVENT_REMOTE_DISCONNECTED:
		{
			[self error:@"Fatal: Remote connection timeout or disconnected" retryHandler:^{
				[self restart];
			}];
		}
			break;
		case Nes::Api::Machine::EVENT_REMOTE_DATA_RATE: //value is address of floating data
		{
			//TODO
		}
			break;
		case Nes::Api::Machine::EVENT_CLIENT_CONNECTED: //value is address of string data
		{
			auto name = (const char*)value;
			if (name)
			{
				auto nameUTF8 = [NSString stringWithUTF8String:name];
				[self.view makeToast:[NSString stringWithFormat:@"%@ has connected", nameUTF8]
							duration:5.
							position:CSToastPositionTop];
			}
		}
			break;
		case Nes::Api::Machine::EVENT_CLIENT_DISCONNECTED:
		{
			auto name = (const char*)value;
			if (name)
			{
				auto nameUTF8 = [NSString stringWithUTF8String:name];
				[self.view makeToast:[NSString stringWithFormat:@"%@ has disconnected", nameUTF8]
							duration:5.
							position:CSToastPositionTop];
			}
		}
			break;
		default:
			//TODO
			break;
	}
}

- (NSString*) constructQuickSaveFilePathForGame: (NSString*)currentGameName slot:(int)slot {
	auto writableDir = getWritableDirectory();
	return [NSString stringWithFormat:@"%@/%@%d.%@.%@", writableDir, QUICK_SAVE_PREFIX, slot, currentGameName, SAVE_FILE_POSTFIX];
}

- (int) parseQuickSaveSlotInName:(NSString*) fileName forGame:(NSString*) currentGameName {
	NSRange   searchedRange = NSMakeRange(0, [fileName length]);
	NSString* pattern = [NSString stringWithFormat:@"%@([0-9]+)\\.\\Q%@\\E\\.%@", QUICK_SAVE_PREFIX, currentGameName, SAVE_FILE_POSTFIX];
	
	NSError  *error = nil;
	
	NSRegularExpression* regex = [NSRegularExpression regularExpressionWithPattern: pattern options:0 error:&error];
	NSArray* matches = [regex matchesInString:fileName options:0 range: searchedRange];
	if (matches == nil || [matches count] != 1)
		return -1;
	
	auto match = (NSTextCheckingResult*)[matches objectAtIndex:0];
	auto slotStrRange = [match rangeAtIndex:1];
	auto slotStr = [fileName substringWithRange:slotStrRange];
	auto slot  = [slotStr intValue];
	
	if (slot < 0 || slot >= MAX_QUICK_SAVES)
		return -1;
	
	return slot;
}

- (void) listQuickSaveFilesForGame: (NSString*) currentGameName completion: (void (^)(NSArray<NSString*>*)) handler
{
	//run in background thread
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		auto writablePath = getWritableDirectory();
		auto writableURL = [NSURL fileURLWithPath:writablePath isDirectory:YES];
		auto contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:writablePath error:nil];
		
		if (contents == nil)
		{
			dispatch_async(dispatch_get_main_queue(), ^{
				handler(nil);
			});
			
			return;
		}
		
		NSMutableArray<NSString*> *sortedList = [NSMutableArray array];
		auto sortCompare = ^(NSString* file1, NSString* file2){
			auto file1URL = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@/%@", writablePath, file1]];
			auto file2URL = [NSURL fileURLWithPath:[NSString stringWithFormat:@"%@/%@", writablePath, file2]];
			
			NSError* error;
			NSDate *file1Date, *file2Date;
			
			//get modification dates
			error = nil;
			[file1URL getResourceValue:&file1Date forKey:NSURLContentModificationDateKey error:&error];
			if (error)
				file1Date = [NSDate dateWithTimeIntervalSince1970:0];//assume very old file on error
			
			error = nil;
			[file2URL getResourceValue:&file2Date forKey:NSURLContentModificationDateKey error:&error];
			if (error)
				file2Date = [NSDate dateWithTimeIntervalSince1970:0];//assume very old file on error
			
			//compare modification dates
			return [file2Date compare:file1Date];
		};
		
		//filter out the correct save files and sort them
		for (NSString* file in contents) {
			NSString* fileName = [file lastPathComponent];
			if ([self parseQuickSaveSlotInName:fileName forGame:currentGameName] != -1)
			{
				auto insertIndex = [sortedList indexOfObject:fileName
											   inSortedRange:NSMakeRange(0, [sortedList count])
													 options:NSBinarySearchingInsertionIndex
											 usingComparator:sortCompare];
				
				[sortedList insertObject:fileName atIndex:insertIndex];
			}
		}
		
		//done
		dispatch_async(dispatch_get_main_queue(), ^{
			handler(sortedList);
		});
	});
}

- (BOOL) handleLoadStateError: (Nes::Result) re {
	if (NES_FAILED(re)){
		std::ostringstream ss;
		ss << re;
		[self error:[NSString stringWithFormat:@"Load saved file error: %s", ss.str().c_str()] fatal:NO];
		return YES;
	}
	
	return NO;
}

- (BOOL) handleSaveStateError: (Nes::Result) re {
	if (NES_FAILED(re)){
		std::ostringstream ss;
		ss << re;
		[self error:[NSString stringWithFormat:@"Save error: %s", ss.str().c_str()] fatal:NO];
		
		return YES;
	}
	return NO;
}

- (void) dismissProgressDialog {
	dismissProgressMsg(self.view);
}

- (void) displayProgressDialog: (NSString*)message {
	[self dismissProgressDialog];
	
	displayProgressMsg(self.view, message, self, @selector(progressDialogCancel:));
}

- (IBAction)progressDialogCancel:(id)sender {
	[self exit];
}

- (void)error: (NSString*)msg {
	[self error:msg fatal:YES];
}

- (void)error: (NSString*)msg fatal: (BOOL)shouldExit{
	if (!_loaded)
	{
		shouldExit = _exiting = YES;//avoid [self start] to be called repeatedly
	}
	
	[self dismissProgressDialog];
	
	//display popup message
	displayErrorMsg(self, @"Error", msg, ^{
		if (shouldExit)
			[self exit];
	});
}

- (void)error: (NSString*)msg retryHandler: (void (^)(void))retryHandler {
	if (!_loaded)
	{
		[self error:msg fatal:YES];
	}
	
	[self dismissProgressDialog];
	
	//display popup message
	displayErrorMsgWithRetryBtn(self, @"Error", msg, retryHandler,
								^{
									[self exit];
								});
}

//override UIViewController
- (void)dismissViewControllerAnimated:(BOOL)flag
						   completion:(void (^)(void))completion {
	[super dismissViewControllerAnimated:flag completion:completion];
	if (_didDidmissCallback)
		_didDidmissCallback();
}

#pragma mark Facebook
- (void) inviteFriendWithGUID: (RakNet::RakNetGUID) myGuid andKey: (uint64_t)key {
	dispatch_async(dispatch_get_main_queue(), ^{
		[self clearAllGameRequests];
		
		FBSDKGameRequestContent *gameRequestContent = [[FBSDKGameRequestContent alloc] init];
		gameRequestContent.message = @"Come play with me!";
		gameRequestContent.data = [NSString stringWithFormat:@"%llu_%llu", key, myGuid.g];
		
		// Assuming self implements <FBSDKGameRequestDialogDelegate>
		[FBSDKGameRequestDialog showWithContent:gameRequestContent delegate:self];
	});
}

- (void)clearAllGameRequests {
	if (_lastRequestRecipients != nil && _lastRequestId != nil) {
#if 0//seems to not have permission to delete the request after it is sent to the recipients
		FBSDKGraphRequestConnection *connection = [[FBSDKGraphRequestConnection alloc] init];
		
		for (NSString* recipient in _lastRequestRecipients) {
			NSString* full_id = [NSString stringWithFormat:@"%@_%@", _lastRequestId, recipient];
			
			FBSDKGraphRequest *graph_request = [[FBSDKGraphRequest alloc]
												initWithGraphPath:[NSString stringWithFormat:@"/%@", full_id]
												parameters:nil
												HTTPMethod:@"DELETE"];
			
			[connection addRequest:graph_request completionHandler:^(FBSDKGraphRequestConnection *connection, id result, NSError *error) {
				
			}];
		}
		
		[connection start];
#endif
		
		_lastRequestId = nil;
		_lastRequestRecipients = nil;
	}
	
}

/*!
 @abstract Sent to the delegate when the game request completes without error.
 @param gameRequestDialog The FBSDKGameRequestDialog that completed.
 @param results The results from the dialog.  This may be nil or empty.
 */
- (void)gameRequestDialog:(FBSDKGameRequestDialog *)gameRequestDialog didCompleteWithResults:(NSDictionary *)results {
	_lastRequestId = [results objectForKey:@"request"];
	_lastRequestRecipients = [results objectForKey:@"to"];
}

/*!
 @abstract Sent to the delegate when the game request encounters an error.
 @param gameRequestDialog The FBSDKGameRequestDialog that completed.
 @param error The error.
 */
- (void)gameRequestDialog:(FBSDKGameRequestDialog *)gameRequestDialog didFailWithError:(NSError *)error {
	[self error:[error localizedDescription]];
}

/*!
 @abstract Sent to the delegate when the game request dialog is cancelled.
 @param gameRequestDialog The FBSDKGameRequestDialog that completed.
 */
- (void)gameRequestDialogDidCancel:(FBSDKGameRequestDialog *)gameRequestDialog {
	[self exit];
}

#pragma mark Touches
- (CGPoint) convertToNesViewCoordinates: (UITouch*) touch
{
	auto scale = [[UIScreen mainScreen] scale];
	CGPoint point = [touch locationInView:self.glview];
	point.x *= scale;
	point.y *= scale;
	
	point.y = g_pSystem->GetScreenHeight() - point.y;
	
	return point;
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
	for (UITouch* touch in touches)
	{
		auto point = [self convertToNesViewCoordinates:touch];
		g_pSystem->OnTouchBegan((__bridge void*) touch, point.x, point.y);
	}
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
	for (UITouch* touch in touches)
	{
		auto point = [self convertToNesViewCoordinates:touch];
		g_pSystem->OnTouchMoved((__bridge void*)touch, point.x, point.y);
	}
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
	for (UITouch* touch in touches)
	{
		auto point = [self convertToNesViewCoordinates:touch];
		g_pSystem->OnTouchEnded((__bridge void*)touch, point.x, point.y);
	}
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
	for (UITouch* touch in touches)
	{
		auto point = [self convertToNesViewCoordinates:touch];
		g_pSystem->OnTouchEnded((__bridge void*)touch, point.x, point.y);
	}
}

#pragma mark filebrowser callbacks
- (void)fileBrowserFinishedPickingFile:(NSData*)file withName:(NSString*)fileName {
	if (file == nil)
		return;
	try {
		Nes::Api::MemInputStream ms((const char*)file.bytes, (size_t)file.length);
		auto re = g_pSystem->LoadState(ms);
		[self handleLoadStateError:re];
	} catch (...){
		[self error:[NSString stringWithFormat:@"Load saved file error: Out of memory"] fatal:NO];
	}
}

- (void)fileBrowserCanceledPickingFile {
	
}
- (void)fileBrowserFinishedSavingFileNamed:(NSString*)fileName {
	
}
- (void)fileBrowserCanceledSavingFile:(NSData*)file {
	
}

- (void)createFileBrowserIfNeeded {
	if (_fileBrowser == nil)
	{
		_fileBrowser = [[PXRFileBrowser alloc] init];
		_fileBrowser.delegate = self;
		_fileBrowser.sortMode = kPXRFileBrowserSortModeNewerFirst;
	}
}

#pragma  mark IBAction
- (IBAction)exitAction:(id)sender {
	[self exit];
}

- (IBAction)saveAction:(id)sender {
	[self createFileBrowserIfNeeded];
	
	try {
		std::stringstream ss(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
		auto re = g_pSystem->SaveState(ss);
		if ([self handleSaveStateError:re])
			return;
		//convert save data to objective-c object
		auto len = ss.tellp();
		NSMutableData* saveData = [NSMutableData dataWithLength:len];
		if (saveData == nil)
		{
			throw std::bad_alloc();
		}
		ss.read((char*)saveData.mutableBytes, len);
		
		//save file dialog
		long long milliseconds = (long long)([[NSDate date] timeIntervalSince1970] * 1000.0);
		NSString* defaultName = [NSString stringWithFormat:@"%s.%lld", g_pSystem->LoadedFileName().c_str(), milliseconds];
		[_fileBrowser saveFile:saveData withType:SAVE_FILE_POSTFIX andDefaultFileName:defaultName];
		[self presentViewController:_fileBrowser animated:YES completion:nil];
		
	} catch (...){
		[self error:[NSString stringWithFormat:@"Save game error: Out of memory"] fatal:NO];
		return;
	}
}

- (IBAction)loadSaveAction:(id)sender {
	[self createFileBrowserIfNeeded];
	
	//load file dialog
	[_fileBrowser browseForFileWithType:SAVE_FILE_POSTFIX];
	[self presentViewController:_fileBrowser animated:YES completion:nil];
}

- (IBAction)quickSaveAction:(id)sender {
	auto currentGame = [NSString stringWithUTF8String:g_pSystem->LoadedFileName().c_str()];
	[self listQuickSaveFilesForGame:currentGame completion:^(NSArray<NSString*>* sortedSaveFiles){
		int slot;
		if (sortedSaveFiles != nil && [sortedSaveFiles count] > 0)  {
			//have existing save files, increase the slot index
			auto file = [sortedSaveFiles objectAtIndex:0];
			slot = [self parseQuickSaveSlotInName:file forGame:currentGame];
			
			slot = (slot + 1) % MAX_QUICK_SAVES;
		}
		else
			slot = 0;
		
		auto fileToSave = [self constructQuickSaveFilePathForGame:currentGame slot:slot];
		
		Nes::Result re;
		try {
			if (fileToSave == nil)
				re = Nes::RESULT_ERR_OUT_OF_MEMORY;
			else
				re = g_pSystem->SaveState([fileToSave UTF8String]);
		} catch (...) {
			re = Nes::RESULT_ERR_OUT_OF_MEMORY;
		}
		
		[self handleSaveStateError:re];
	}];
}

- (IBAction)quickLoadAction:(id)sender {
	auto currentGame = [NSString stringWithUTF8String:g_pSystem->LoadedFileName().c_str()];
	[self listQuickSaveFilesForGame:currentGame completion:^(NSArray<NSString*>* sortedSaveFiles){
		if (sortedSaveFiles != nil && [sortedSaveFiles count] > 0) {
			//load the latest one
			auto file = [sortedSaveFiles objectAtIndex:0];
			auto fileFullPath = [NSString stringWithFormat:@"%@/%@", getWritableDirectory(), file];
			
			Nes::Result re;
			try {
				if (fileFullPath == nil)
					re = Nes::RESULT_ERR_OUT_OF_MEMORY;
				else
					re = g_pSystem->LoadState([fileFullPath UTF8String]);
			} catch (...) {
				re = Nes::RESULT_ERR_OUT_OF_MEMORY;
			}
			
			[self handleLoadStateError:re];
		}
	}];
}

@end
