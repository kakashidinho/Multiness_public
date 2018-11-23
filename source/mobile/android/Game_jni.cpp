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

#include "JniUtils.hpp"
#include "AudioDriver.hpp"
#include "Game_jni.hpp"

#include "../NesSystemWrapper.hpp"
#include "../RendererGLES.hpp"
#include "../InputGLES.hpp"

#include "../../core/api/NstApiEmulator.hpp"
#include "../../core/api/NstApiVideo.hpp"
#include "../../core/api/NstApiSound.hpp"
#include "../../core/api/NstApiInput.hpp"
#include "../../core/api/NstApiMachine.hpp"
#include "../../core/api/NstApiUser.hpp"
#include "../../core/api/NstApiFds.hpp"
#include "../../core/api/NstApiDipSwitches.hpp"
#include "../../core/api/NstApiRewinder.hpp"
#include "../../core/api/NstApiCartridge.hpp"
#include "../../core/api/NstApiMovie.hpp"
#include "../../core/api/NstApiNsf.hpp"
#include "../../core/api/NstApiMemoryStream.hpp"

#include "../../remote_control/ConnectionHandlerRakNet.hpp"


#include "RemoteController/Timer.h"
#include "RemoteController/ConnectionHandler.h"

#include "RakPeerInterface.h"

#include <android/log.h>
#include <android/asset_manager_jni.h>

#include <HQAssetDataStreamAndroid.h>

#include <sstream>
#include <fstream>
#include <atomic>
#include <cinttypes>

using namespace Nes;

// TODO: change your NAT server address here, since this server could be closed at anytime
#ifdef NAT_SERVER_ADDRESS
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
static const char NAT_SERVER[] = VALUE(NAT_SERVER_ADDRESS); // "192.168.2.112";
#else
static const char NAT_SERVER[] = "125.212.218.120";
#endif
static const int NAT_SERVER_PORT = 61111;

static const char* PROXY_SERVER = nullptr; // use your own relay server
static const int PROXY_SERVER_PORT = 0;

static const char GET_SETTINGS_CLASS_METHOD_NAME[] = "getSettingsClass";
static const char RUN_ON_MAINTHREAD_METHOD_NAME[] = "runOnMainThread";
static const char ON_LAN_SERVER_DISCOVERED_METHOD_NAME[] = "onLanServerDiscovered";
static const char GET_LAN_IP_METHOD_NAME[] = "getHostIPAddress";
static const char GET_ASSETS_METHOD_NAME[] = "getAssetsManagerObject";

static const char ERROR_CALLBACK_METHOD_NAME[] = "fatalError";
static const char MACHINE_EVENT_CALLBACK_METHOD_NAME[] = "machineEventCallback";
static const char EXIT_GAME_ACTIVITY_METHOD_NAME[] = "finishGamePage";
static const char RUN_ON_GAMETHREAD_METHOD_NAME[] = "runOnGameThread";
static const char DISPLAY_INVITE_DIALOG_METHOD_NAME[] = "displayInviteDialog";
static const char CLIENT_ABOUT_TO_CONNECT_METHOD_NAME[] = "clientAboutToConnectToRemote";
static const char CLIENT_CONNECT_TEST_CALLBACK_METHOD_NAME[] = "clientConnectivityTestCallback";
static const char CREATE_PUB_SERVER_META_METHOD_NAME[] = "createPublicServerMetaData";

static const char IS_VOICE_ENABLED_METHOD_NAME[] = "isVoiceChatEnabled";

static jclass g_BaseActivityClass = NULL;
static jmethodID g_getSettingsClassMethodID;
static jmethodID g_runOnMainThreadMethodID;
static jmethodID g_onLanServerDiscoveredMethodID;
static jmethodID g_getLanIpAddressMethodID;
static jmethodID g_getAssetsMethodID;

static jclass g_GameSurfaceViewClass = NULL;
static jmethodID g_errorCallbackMethodID;
static jmethodID g_machineEventIntCallbackMethodID;
static jmethodID g_machineEventFloatCallbackMethodID;
static jmethodID g_machineEventLongCallbackMethodID;
static jmethodID g_machineEventStringCallbackMethodID;
static jmethodID g_exitGameActivityMethodID;
static jmethodID g_runOnGameThreadMethodID;
static jmethodID g_displayInviteDialogMethodID;
static jmethodID g_clientAboutToConnectToRemoteMethodID;
static jmethodID g_clientConnectTestCallbackMethodID;
static jmethodID g_createPublicServerMetaDataMethodID;

static jmethodID g_isVoiceEnabledMethodID;

enum CONN_HANDLER_TYPE {
	CONN_HANDLER_TYPE_LAN_SERVER = 1,
	CONN_HANDLER_TYPE_LAN_CLIENT,
	CONN_HANDLER_TYPE_INTERNET_SERVER,
	CONN_HANDLER_TYPE_INTERNET_CLIENT,
};

//helpers prototype
static void ExitGameActivity(jobject javaHandle);
static jclass GetSettingsClass();
static bool IsSettingsVoiceEnabled();
static void NotifyJavaOnLanServerDiscovered(uint64_t request_id, const char* addr, int reliablePort, int unreliablePort, const char* desc);

//NesSystem
class NesSystem: public NesSystemWrapper, public HQRemote::SocketServerDiscoverClientHandler::DiscoveryDelegate {
public:
	NesSystem(std::istream* dbStream, Callback::VLogCallback logCallback)
	: NesSystemWrapper(dbStream, logCallback),
	  m_soundDriver(m_emulator), m_lanServerDiscoveryHandler(this)
	{
		m_renderer = std::unique_ptr<Video::IRenderer>(new Video::GL::Renderer(m_emulator));

		ResetSyncTimer();
	}
	
	~NesSystem() {
		m_lanServerDiscoveryHandler.stop();
	}
	
	void SetESVersion(int versionMajor)
	{
		auto glRenderer = static_cast<Video::GL::Renderer*>(m_renderer.get());
		glRenderer->SetESVersion(versionMajor);
	}
	
	void SetAudioOutputVolume(float gain) {
		m_soundDriver.SetOutputVolume(gain);
	}

	void AudioRecordPermissionChanged(bool hasPermission) {
		m_soundDriver.InputPermissionChanged(hasPermission);
	}
	
	void EnableAudioInput(bool enable) {
		auto settingsVoiceEnabled = IsSettingsVoiceEnabled();
		bool final_enable = enable && settingsVoiceEnabled;
		
		__android_log_print(ANDROID_LOG_DEBUG, "Nes", "EnableAudioInput() request_enable=%d & settingsVoiceEnabled=%d => %d",
							(int)enable, (int)settingsVoiceEnabled, (int)final_enable);
		
		m_soundDriver.EnableInput(final_enable);
	}
	
	void OnPause() {
		m_soundDriver.Pause();
	}
	
	void OnResume() {
		ResetSyncTimer();
		m_soundDriver.Resume();
	}
	
	void ResetSyncTimer() {
		clock_gettime(CLOCK_MONOTONIC, &m_nextExecTime);
	}
	
	void Execute(){
		bool skip_frame = false;
#if 1//disable/enable frame timing synchronisation
		const auto frameRate = m_soundDriver.GetExpectedFrameRate();
		const auto interval_ns = 1000000000 / frameRate;
		
		timespec curTime;

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &m_nextExecTime, 0);
		
		clock_gettime(CLOCK_MONOTONIC, &curTime);
		int64_t diff_ns = (curTime.tv_sec - m_nextExecTime.tv_sec) * 1000000000 + (curTime.tv_nsec - m_nextExecTime.tv_nsec);
		
		//__android_log_print(ANDROID_LOG_DEBUG, "Nes", "Execute(), sleep delayed by %lld (ns)", diff_ns);
		
		//calculate next update time
		if (diff_ns > interval_ns)
			m_nextExecTime = curTime;
		else if (diff_ns > interval_ns / 2)
			skip_frame = true;
		
		m_nextExecTime.tv_nsec += interval_ns;
		if (m_nextExecTime.tv_nsec >= 1000000000)
		{
			m_nextExecTime.tv_nsec -= 1000000000;
			m_nextExecTime.tv_sec ++;
		}
#endif
		//execute
		NesSystemWrapper::Execute(/*skip_frame*/);
	}
	
	void StartLanServerDiscoveryService() {
		m_lanServerDiscoveryHandler.start();
	}
	
	void StopLanServerDiscoveryService() {
		m_lanServerDiscoveryHandler.stop();
	}
	
	void FindLanServers(uint64_t request_id) {
		m_lanServerDiscoveryHandler.findOtherServers(request_id);
	}
private:
	//DiscoveryDelegate implementation
	virtual void onNewServerDiscovered(HQRemote::SocketServerDiscoverClientHandler* handler, uint64_t request_id, const char* addr, int reliablePort, int unreliablePort, const char* desc) override
	{
		NotifyJavaOnLanServerDiscovered(request_id, addr, reliablePort, unreliablePort, desc);
	}

	timespec m_nextExecTime;

	Sound::Android::AudioDriver m_soundDriver;
	
	HQRemote::SocketServerDiscoverClientHandler m_lanServerDiscoveryHandler;
};

//LoadRemoteAsyncArgs
struct LoadRemoteAsyncArgs {
	std::shared_ptr<HQRemote::IConnectionHandler> connHandler;
	jstring clientName;
};

//helpers
static void ExitGameActivity(jobject javaHandle)
{
	auto env = Jni::GetCurrenThreadJEnv();
	env->CallVoidMethod(javaHandle, g_exitGameActivityMethodID);
}

static jclass GetSettingsClass()
{
	auto env = Jni::GetCurrenThreadJEnv();
	return (jclass)env->CallStaticObjectMethod(g_BaseActivityClass, g_getSettingsClassMethodID);
}

static bool IsSettingsVoiceEnabled() {
	auto env = Jni::GetCurrenThreadJEnv();
	
	auto jSettingsClass = GetSettingsClass();
	
	bool re = env->CallStaticBooleanMethod(jSettingsClass, g_isVoiceEnabledMethodID);
	
	env->DeleteLocalRef(jSettingsClass);
	
	return re;
}

void NotifyJavaOnLanServerDiscovered(uint64_t request_id, const char* addr, int reliablePort, int unreliablePort, const char* desc) {
	if (unreliablePort != reliablePort + 1)
		return;//not what we expected
	
	auto env = Jni::GetCurrenThreadJEnv();
	jlong jrequest_id = (jlong)request_id;
	auto jaddr = env->NewStringUTF(addr);
	if (env->ExceptionOccurred())
	{
		jaddr = 0;
		env->ExceptionDescribe();
		env->ExceptionClear();
	}
	
	auto jdesc = env->NewStringUTF(desc);
	if (env->ExceptionOccurred())
	{
		jdesc = 0;
		env->ExceptionDescribe();
		env->ExceptionClear();
	}
	
	env->CallStaticVoidMethod(g_BaseActivityClass, g_onLanServerDiscoveredMethodID, jrequest_id, jaddr, reliablePort, jdesc);
	
	if (jaddr)
		env->DeleteLocalRef(jaddr);
	if (jdesc)
		env->DeleteLocalRef(jdesc);
}

static inline std::string GetLanIpAddress() {
	std::string address;
	auto env = Jni::GetCurrenThreadJEnv();
	jstring jaddress = (jstring)env->CallStaticObjectMethod(g_BaseActivityClass, g_getLanIpAddressMethodID);
	if (jaddress) {
		// copy to std::string
		auto caddress = env->GetStringUTFChars(jaddress, NULL);
		try {
			address = caddress;
		} catch (...) {
			address.clear();
		}

		env->ReleaseStringUTFChars(jaddress, caddress);

		env->DeleteLocalRef(jaddress);
	}

	return address;
}

typedef void (* async_func_t) (void* args);
typedef void (* async_func2_t) (jlong nativePtr, void* args);

static void RunOnMainThread(async_func_t functionAddr, void* args) {
	auto env = Jni::GetCurrenThreadJEnv();
	env->CallStaticVoidMethod(g_BaseActivityClass, g_runOnMainThreadMethodID, (jlong)functionAddr, (jlong)args);
}

static void RunOnGameThread(jobject javaHandle, async_func2_t functionAddr, void* args) {
	auto env = Jni::GetCurrenThreadJEnv();
	env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_runOnGameThreadMethodID, javaHandle, (jlong)functionAddr, (jlong)args);
}

static void ErrorCallback(const char* message)
{
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_errorCallbackMethodID)
		return;
	auto jmessage = env->NewStringUTF(message);
	
	if (jmessage)
	{
		env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_errorCallbackMethodID, jmessage);
		
		env->DeleteLocalRef(jmessage);
	}
}

static void LogCallback(const char* format, va_list args)
{
	__android_log_vprint(ANDROID_LOG_DEBUG, "Nes", format, args);
}

static bool InitInternetServerConnectionInfo(const RakNet::RakNetGUID& my_rakGuid, uint64_t key, unsigned short lanPort, char* serverInfo, size_t serverInfoBufSize) {
	if (serverInfoBufSize < 256)
		return false;

	// server info for accepting connection = "<key>_<guid>_<lan ip>_<lan port>"
	auto writtenBytes = sprintf(serverInfo, "%" PRIu64 "_", key);
	if (writtenBytes <= 0)
		return false;//TODO: error handling

	//concatenate with guid
	my_rakGuid.ToString(serverInfo + writtenBytes);

	writtenBytes = strlen(serverInfo);

	// append LAN address & port
	auto lanIp = GetLanIpAddress();
	if (lanIp.size()) {
		writtenBytes = sprintf(serverInfo + writtenBytes, "_%s_%d", lanIp.c_str(), lanPort);
		if (writtenBytes <= 0)
			return false;//TODO: error handling
	}

	return true;
}

static void DisplayInviteDialog(const RakNet::RakNetGUID& my_rakGuid, uint64_t key, unsigned short lanPort, int context) {
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_displayInviteDialogMethodID)
		return;
	
	//invite data
	char inviteDataString[256];

	if (!InitInternetServerConnectionInfo(my_rakGuid, key, lanPort, inviteDataString, sizeof(inviteDataString)))
		return;
	
	auto jinviteDataString = env->NewStringUTF(inviteDataString);
	if (jinviteDataString)
	{
		env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_displayInviteDialogMethodID, jinviteDataString, context);
		
		env->DeleteLocalRef(jinviteDataString);
	}
}

static inline std::string CreatePublicServerMetaData(const char* publicName, const char* inviteDataString, int context) {
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_createPublicServerMetaDataMethodID)
		return "";

	HQRemote::JStringRef jinviteDataString = env->NewStringUTF(inviteDataString);
	HQRemote::JStringRef jpublicName = env->NewStringUTF(publicName);
	if (jinviteDataString && jpublicName)
	{
		auto jstr = (jstring)env->CallStaticObjectMethod(g_GameSurfaceViewClass, g_createPublicServerMetaDataMethodID, jpublicName.get(), jinviteDataString.get(), context);

		std::string re_str;

		if (jstr) {
			// convert to C string
			const char* cstr;
			if ((cstr = env->GetStringUTFChars(jstr, NULL)) != NULL)
			{
				// convert to C++ string
				try {
					re_str = cstr;
				} catch (...) {
					re_str = "";
				}
				env->ReleaseStringUTFChars(jstr, cstr);
			}

			env->DeleteLocalRef(jstr);
		}

#ifdef DEBUG
		__android_log_print(ANDROID_LOG_DEBUG, "Nes", "Created Room meta data=%s\n", re_str.c_str());
#endif

		return re_str;
	}

	return "";
}

static bool ParseInviteData(const char* invite_data,
							RakNet::RakNetGUID& remoteGuid,
							uint64_t &inviteKey,
							std::string& lanIp,
							unsigned short& lanPort) {

	struct Free {
		void operator()(char* ptr) {
			free(ptr);
		}
	};

	std::unique_ptr<char, Free> invite_data_copy(strdup(invite_data));
	if (!invite_data_copy)
		return false;

	// invite data = "<key>_<guid>[_<lan ip>_<lan port>]"

	char* tok_ctx = nullptr;
	// key
	auto token = strtok_r(invite_data_copy.get(), "_", &tok_ctx);
	if (token == nullptr || sscanf(token, "%" PRIu64 "", &inviteKey) != 1)
		return false;

	// guid
	token = strtok_r(nullptr, "_", &tok_ctx);
	if (token == nullptr || sscanf(token, "%" PRIu64 "", &remoteGuid.g) != 1)
		return false;

	// lan ip
	token = strtok_r(nullptr, "_", &tok_ctx);
	if (!token)
		return true; // this is optional
	try {
		lanIp = token; // convert to std::string
	} catch (...) {
		lanIp.clear();
	}

	// lan port
	token = strtok_r(nullptr, "_", &tok_ctx);
	if (!token)
		return true; // this is optional

	int port = 0;
	if (sscanf(token, "%d", &port) == 1)
		lanPort = (unsigned short) port;


	return true;
}

static void ClientAboutToConnectToRemote(const char* invite_id, const RakNet::RakNetGUID& remote_guid, int context) {
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_clientAboutToConnectToRemoteMethodID)
		return;
	
	char cremote_guid_str[128];
	remote_guid.ToString(cremote_guid_str);
	
	auto jguid = env->NewStringUTF(cremote_guid_str);
	if (jguid)
	{
		auto jinvite_id = env->NewStringUTF(invite_id);
		
		if (jinvite_id)
		{
			env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_clientAboutToConnectToRemoteMethodID, jinvite_id, jguid, context);
			
			env->DeleteLocalRef(jinvite_id);
		}
		
		env->DeleteLocalRef(jguid);
	}//if (jguid)
}

static void InvokeClientConnectivityResultCallback(const char* invite_id, const RakNet::RakNetGUID& remote_guid, jobject callback, bool result) {
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_clientConnectTestCallbackMethodID)
		return;

	char cremote_guid_str[128];
	remote_guid.ToString(cremote_guid_str);

	auto jguid = env->NewStringUTF(cremote_guid_str);
	if (jguid)
	{
		auto jinvite_id = env->NewStringUTF(invite_id);

		if (jinvite_id)
		{
			env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_clientConnectTestCallbackMethodID, jinvite_id, jguid, callback, result ? JNI_TRUE : JNI_FALSE);

			env->DeleteLocalRef(jinvite_id);
		}

		env->DeleteLocalRef(jguid);
	}//if (jguid)
}

static void MachineEventJavaCallback(jint event, jint value)
{
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_machineEventIntCallbackMethodID)
		return;
	
	env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_machineEventIntCallbackMethodID,
							  event, value);
}

static void MachineEventJavaCallback(jint event, float value)
{
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_machineEventFloatCallbackMethodID)
		return;
	
	env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_machineEventFloatCallbackMethodID,
							  event, value);
}

static void MachineEventJavaCallback(jint event, jlong value)
{
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_machineEventLongCallbackMethodID)
		return;
	
	env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_machineEventLongCallbackMethodID,
							  event, value);
}

static void MachineEventJavaCallback(jint event, const char* stringValue)
{
	auto env = Jni::GetCurrenThreadJEnv();
	if (!env || !g_machineEventStringCallbackMethodID)
		return;
	
	jbyteArray jvalue = NULL;
	if (stringValue != NULL)
	{
		size_t stringLen = strlen(stringValue);
		jvalue = env->NewByteArray(stringLen);
		env->SetByteArrayRegion(jvalue, 0, stringLen, (const jbyte*)stringValue);
	}
	
	env->CallStaticVoidMethod(g_GameSurfaceViewClass, g_machineEventStringCallbackMethodID,
							  event, jvalue);
		
	if (jvalue)
		env->DeleteLocalRef(jvalue);
}

static void MachineEventCallback(Api::Machine::Event event, Result value)
{
	//notify java layer
	switch (event) {
	case Api::Machine::EVENT_LOAD:
		//ignored, we handle the return code of Machine::Load() directly
		break;
	case Api::Machine::EVENT_REMOTE_DATA_RATE: //value is address of floating data
	{
		auto dataRate = *(float*)(intptr_t)value;
		MachineEventJavaCallback(event, dataRate);
	}
		break;
	case Api::Machine::EVENT_REMOTE_CONNECTED: //value is address of string data
	case Api::Machine::EVENT_REMOTE_CONNECTION_INTERNAL_ERROR:
	case Api::Machine::EVENT_CLIENT_CONNECTED:
	case Api::Machine::EVENT_CLIENT_DISCONNECTED:
	{
		auto stringValue = (const char*)(intptr_t)value;
		MachineEventJavaCallback(event, stringValue);
	}
		break;
	case Api::Machine::EVENT_REMOTE_MESSAGE:
	{
		auto messageData = (Api::Machine::RemoteMsgEventData*)(intptr_t)value;
		MachineEventJavaCallback(event, messageData->message);
	}
		break;
	case Api::Machine::EVENT_REMOTE_MESSAGE_ACK:
	{
		uint64_t id = *(uint64_t*)(intptr_t)value;
		MachineEventJavaCallback(event, (jlong)id);
	}
		break;
	default:
		MachineEventJavaCallback(event, (jint)value);
		break;
	}
}


/*-----------  JNI -----------*/
extern "C" {
	/*----------- BaseActivity ---------*/
	JNIEXPORT void JNICALL 
	Java_com_hqgame_networknes_BaseActivity_cacheJVMNative(JNIEnv *env, jobject activity) {
		//cache Java VM
		static bool cachedJVM = false;
		if (!cachedJVM) {
			JavaVM* jvm;
			env->GetJavaVM(&jvm);
			
			Jni::SetJVM(jvm);//cache JVM for JniUtils
			
			cachedJVM = true;
		}
	}
	
	JNIEXPORT void JNICALL 
	Java_com_hqgame_networknes_BaseActivity_onSignedInNative(JNIEnv *env, jobject activity, jboolean signedIn) {
		//TODO
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onCreatedNative(JNIEnv *env, jobject activity, jobject savedInstanceState) {
		//get class's methods
		if (g_BaseActivityClass == NULL)
		{
			auto Clazz = env->GetObjectClass(activity);
			g_BaseActivityClass = (jclass)env->NewGlobalRef(Clazz);
			
			g_getSettingsClassMethodID = env->GetStaticMethodID(g_BaseActivityClass, GET_SETTINGS_CLASS_METHOD_NAME, "()Ljava/lang/Class;");
			g_runOnMainThreadMethodID = env->GetStaticMethodID(g_BaseActivityClass, RUN_ON_MAINTHREAD_METHOD_NAME, "(JJ)V");
			g_onLanServerDiscoveredMethodID = env->GetStaticMethodID(g_BaseActivityClass, ON_LAN_SERVER_DISCOVERED_METHOD_NAME, "(JLjava/lang/String;ILjava/lang/String;)V");
			g_getLanIpAddressMethodID = env->GetStaticMethodID(g_BaseActivityClass, GET_LAN_IP_METHOD_NAME, "()Ljava/lang/String;");
			g_getAssetsMethodID = env->GetStaticMethodID(g_BaseActivityClass, GET_ASSETS_METHOD_NAME, "()Ljava/lang/Object;");

			// Settings class's method
			auto jSettingsClass = GetSettingsClass();
			
			g_isVoiceEnabledMethodID = env->GetStaticMethodID(jSettingsClass, IS_VOICE_ENABLED_METHOD_NAME, "()Z");
			
			env->DeleteLocalRef(jSettingsClass);
		}
	}
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onDestroyedNative(JNIEnv *env, jobject activity) {
		
	}
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onResumedNative(JNIEnv *env, jobject activity) {
		
	}
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onPausedNative(JNIEnv *env, jobject activity) {
		
	}
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onStartedNative(JNIEnv *env, jobject activity) {
		
	}
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onStoppedNative(JNIEnv *env, jobject activity) {
		
	}
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onSaveInstanceStateNative(JNIEnv *env, jobject activity, jobject state) {
		
	}
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_onActivityResultNative(JNIEnv *env, jobject activity, int requestCode, int resultCode, jobject data) {
		
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_BaseActivity_invokeNativeFunction(JNIEnv *env, jobject activity, jlong nativeFuncPtr, jlong nativeFuncArgs)
	{
		auto function = (async_func_t)nativeFuncPtr;
		void* args = (void*)nativeFuncArgs;
		
		function(args);
	}
	
	/*----------------- GameSurfaceView -----------*/
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_initNative(JNIEnv *env, jclass clazz) {
		//init java methods
		if (g_GameSurfaceViewClass == NULL)
		{
			g_GameSurfaceViewClass = (jclass)env->NewGlobalRef(clazz);

			g_errorCallbackMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, ERROR_CALLBACK_METHOD_NAME, "(Ljava/lang/String;)V");
			g_machineEventIntCallbackMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, MACHINE_EVENT_CALLBACK_METHOD_NAME, "(II)V");
			g_machineEventFloatCallbackMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, MACHINE_EVENT_CALLBACK_METHOD_NAME, "(IF)V");
			g_machineEventLongCallbackMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, MACHINE_EVENT_CALLBACK_METHOD_NAME, "(IJ)V");
			g_machineEventStringCallbackMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, MACHINE_EVENT_CALLBACK_METHOD_NAME, "(I[B)V");

			g_exitGameActivityMethodID = env->GetMethodID(g_GameSurfaceViewClass, EXIT_GAME_ACTIVITY_METHOD_NAME, "()V");
			g_runOnGameThreadMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, RUN_ON_GAMETHREAD_METHOD_NAME, "(Landroid/opengl/GLSurfaceView;JJ)V");

			g_displayInviteDialogMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, DISPLAY_INVITE_DIALOG_METHOD_NAME, "(Ljava/lang/String;I)V");
			g_clientAboutToConnectToRemoteMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, CLIENT_ABOUT_TO_CONNECT_METHOD_NAME, "(Ljava/lang/String;Ljava/lang/String;I)V");
			g_clientConnectTestCallbackMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, CLIENT_CONNECT_TEST_CALLBACK_METHOD_NAME, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Object;Z)V");
			g_createPublicServerMetaDataMethodID = env->GetStaticMethodID(g_GameSurfaceViewClass, CREATE_PUB_SERVER_META_METHOD_NAME, "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;");

		}//if (g_GameSurfaceViewClass == NULL)
	}

	JNIEXPORT jlong JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_initGameNative(JNIEnv *env, jclass clazz, jbyteArray databaseData)
	{
		static_assert(sizeof(jlong) >= sizeof(void*), "Unsupported sizeof pointer");

		//load database
		std::istream *dbStream = NULL;
		jbyte* cdatabaseData = NULL;
		if (databaseData != NULL && (cdatabaseData = env->GetByteArrayElements(databaseData, NULL)) != NULL)
		{
			try {
				dbStream = new Api::MemInputStream((const char*)cdatabaseData, env->GetArrayLength(databaseData));
			} catch (...)
			{
				dbStream = NULL;
			}
			
			env->ReleaseByteArrayElements(databaseData, cdatabaseData, 0);
		}
		
		//create native system
		auto system = new NesSystem(dbStream, [](const char* format, va_list args) {
			LogCallback(format, args);
		});
		
		delete dbStream;
		
		//register callbacks
		system->SetMachineEventCallback([](Api::Machine::Event event, Result result){
			MachineEventCallback(event, result);
		});
		
#if 0
		system->SetErrorCallback([](const char* msg){
			ErrorCallback(msg);
		});
#endif
		
		system->SetInput(new Input::GL::InputDPad());
		
		return (jlong)system;
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_destroyGameNative(JNIEnv *env, jclass clazz, jlong nativePtr)
	{
		//unregister callbacks
		Api::Machine::eventCallback.Set(NULL, NULL);
		
		auto system = (NesSystem*)nativePtr;
		delete system;
		
		if (g_GameSurfaceViewClass != NULL)
		{
			env->DeleteGlobalRef(g_GameSurfaceViewClass);
			g_GameSurfaceViewClass = NULL;
		}
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_verifyGameNative(JNIEnv *env, jclass clazz, jlong nativePtr, jstring path)
	{
		auto system = (NesSystem*)nativePtr;
		std::string stdpath = "";
		const char* cpath;
		if (path != NULL && (cpath = env->GetStringUTFChars(path, NULL)) != NULL)
		{
			stdpath = cpath;
			env->ReleaseStringUTFChars(path, cpath);
		}
		
		return NES_SUCCEEDED(system->VerifyGame(stdpath));
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_isGameLoadedNative(JNIEnv *env, jclass clazz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		return system->Loaded();
	}
	
	JNIEXPORT jstring JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_loadedGameNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		try {
			auto path = system->LoadedFile();
			return env->NewStringUTF(path.c_str());
		} catch (...) {
			return NULL;
		}
	}

	JNIEXPORT jstring JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_loadedGameNameNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		try {
			auto path = system->LoadedFileName();
			return env->NewStringUTF(path.c_str());
		} catch (...) {
			return NULL;
		}
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_loadAndStartGameNative(JNIEnv *env, jobject thiz, jlong nativePtr, jstring path)
	{
		auto system = (NesSystem*)nativePtr;
		
		std::string stdpath = "";
		const char* cpath;
		if (path != NULL && (cpath = env->GetStringUTFChars(path, NULL)) != NULL)
		{
			stdpath = cpath;
			env->ReleaseStringUTFChars(path, cpath);
		}
		
		auto re = system->LoadGame(stdpath);
		
		MachineEventJavaCallback(Api::Machine::EVENT_LOAD, (jint)re);
		
		bool succeeded = NES_SUCCEEDED(re);
		
		//only enable sound input (mic) when remote control is enabled
		system->EnableAudioInput(system->RemoteControllerEnabled(1));
		
		return succeeded;
	}
	
	static jboolean GameSurfaceView_loadRemoteGeneric(JNIEnv *env, jlong nativePtr, std::shared_ptr<HQRemote::IConnectionHandler> connHandler, jstring clientName)
	{
		auto system = (NesSystem*)nativePtr;
		
		const char* cname = clientName != NULL ? env->GetStringUTFChars(clientName, NULL) : NULL;
		
		auto re = system->LoadRemote(connHandler, cname);
		
		if (cname)
			env->ReleaseStringUTFChars(clientName, cname);
		
		if (re)
			system->EnableAudioInput(true);
		
		return re;
	}
	
	static void GameSurfaceView_loadRemoteGenericAsync(jlong nativePtr, void *args)
	{
		auto env = Jni::GetCurrenThreadJEnv();
		auto system = (NesSystem*)nativePtr;
		auto asyncArgs = (LoadRemoteAsyncArgs*)args;
		
		auto re = GameSurfaceView_loadRemoteGeneric(env, nativePtr, asyncArgs->connHandler, asyncArgs->clientName);
		
		//clean up arguments
		if (asyncArgs->clientName)
			env->DeleteGlobalRef(asyncArgs->clientName);
		delete asyncArgs;
	}
	
	static RakNet::RakNetGUID guidFromJString(JNIEnv *env, jstring id) {
		RakNet::RakNetGUID guid;
		
		guid.g = RakNet::RakPeerInterface::Get64BitUniqueRandomNumber();//default random GUID
		
		if (id) {
			auto cid = env->GetStringUTFChars(id, NULL);
			size_t length = strlen(cid);

			// trim starting non-digit characters
			auto adjusted_cid = cid;
			size_t startDigitIdx = 0;
			while (cid[startDigitIdx] < '0' || cid[startDigitIdx] > '9')
				startDigitIdx++;

			adjusted_cid = cid + startDigitIdx;

			RakNet::RakNetGUID tmpGuid;
			tmpGuid.FromString(adjusted_cid);

			// possibly overflow
			if (tmpGuid == RakNet::UNASSIGNED_RAKNET_GUID)
			{
				try {
					// reduce number of digits
					std::string cppid = adjusted_cid;
					cppid.resize(18);

					tmpGuid.FromString(cppid.c_str());
				} catch (...) {
					tmpGuid = RakNet::UNASSIGNED_RAKNET_GUID;
				}
			}
			// should be valid
			if (tmpGuid != RakNet::UNASSIGNED_RAKNET_GUID)
				guid = tmpGuid;
			
			if (cid)
				env->ReleaseStringUTFChars(id, cid);
		}
		
		return guid;
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_loadRemoteNative(JNIEnv *env, jobject thiz, jlong nativePtr, jstring ip, jint port, jstring clientName)
	{
		auto system = (NesSystem*)nativePtr;
		
		const char* cname = clientName != NULL ? env->GetStringUTFChars(clientName, NULL) : NULL;
		const char* cip = env->GetStringUTFChars(ip, NULL);
		
		auto re = system->LoadRemote(cip, port, cname);
		
		env->ReleaseStringUTFChars(ip, cip);
		if (cname)
			env->ReleaseStringUTFChars(clientName, cname);

		if (re) {
			auto connHandler = system->GetRemoteControllerClientConnHandler();
			if (connHandler)
				connHandler->setTag(CONN_HANDLER_TYPE_LAN_CLIENT);

			system->EnableAudioInput(true);
		}
		
		return re;
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_loadRemoteInternetNative(JNIEnv *env, jobject thiz, jlong nativePtr, jstring invite_id, jstring invite_data, jstring clientGUID, jstring clientName, jint context)
	{
		auto cinvite_id = env->GetStringUTFChars(invite_id, NULL);
		auto cinvite_data = env->GetStringUTFChars(invite_data, NULL);
		
		auto invite_id_ref = std::make_shared<std::string>(cinvite_id);
		
		RakNet::RakNetGUID cremoteGuid(0);
		RakNet::RakNetGUID cclientGuid(0);
		uint64_t key = 0;
		std::string cremoteLanIp;
		unsigned short cremoteLanPort;
		
		ParseInviteData(cinvite_data, cremoteGuid, key, cremoteLanIp, cremoteLanPort);//TODO: parsing error handling
		
		//convert client guid's string to value
		cclientGuid = guidFromJString(env, clientGUID);
		
		auto connHandler = std::make_shared<Remote::ConnectionHandlerRakNetClient>(&cclientGuid,
																				   NAT_SERVER, NAT_SERVER_PORT,
																				   PROXY_SERVER, PROXY_SERVER_PORT,
																				   cremoteGuid,
																				   std::move(cremoteLanIp),
																				   cremoteLanPort,
																				   key,
																				   false,
																				   [invite_id_ref, context] (const Remote::ConnectionHandlerRakNet* handler){
																					   //successfully connected to central server
																					   auto remote_guid = (static_cast<const Remote::ConnectionHandlerRakNetClient*>(handler))->getRemoteGUID();
																					   
																					   ClientAboutToConnectToRemote(invite_id_ref->c_str(), remote_guid, context);
																				   });

		connHandler->setTag(CONN_HANDLER_TYPE_INTERNET_CLIENT);
		
		env->ReleaseStringUTFChars(invite_id, cinvite_id);
		env->ReleaseStringUTFChars(invite_data, cinvite_data);
		
		return GameSurfaceView_loadRemoteGeneric(env, nativePtr, connHandler, clientName);
	}

	JNIEXPORT jlong JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_createRemoteInternetConnectivityTestNative(JNIEnv *env, jobject thiz, jstring invite_id, jstring invite_data, jstring clientGUID, jobject jCallback)
	{
		auto cinvite_id = invite_id ? env->GetStringUTFChars(invite_id, NULL) : NULL;
		auto cinvite_data = env->GetStringUTFChars(invite_data, NULL);

		auto invite_id_ref = std::make_shared<std::string>(cinvite_id ? cinvite_id : "");

		auto jCallback_ref = std::make_shared<HQRemote::JObjectRef>(env->NewGlobalRef(jCallback));
		if (!jCallback_ref->get()) {
			if (env->ExceptionCheck()) {
				env->ExceptionDescribe();
				env->ExceptionClear();
			}
			return 0;
		}

		RakNet::RakNetGUID cremoteGuid(0);
		RakNet::RakNetGUID cclientGuid(0);
		uint64_t key = 0;
		std::string cremoteLanIp;
		unsigned short cremoteLanPort;

		ParseInviteData(cinvite_data, cremoteGuid, key, cremoteLanIp, cremoteLanPort);//TODO: parsing error handling

		//convert client guid's string to value
		cclientGuid = guidFromJString(env, clientGUID);

		auto connHandler = new Remote::ConnectionHandlerRakNetClient(&cclientGuid,
																	   NAT_SERVER, NAT_SERVER_PORT,
																	   PROXY_SERVER, PROXY_SERVER_PORT,
																	   cremoteGuid,
																	   std::move(cremoteLanIp),
																	   cremoteLanPort,
																	   key,
																	   true);


		if (invite_id)
			env->ReleaseStringUTFChars(invite_id, cinvite_id);
		env->ReleaseStringUTFChars(invite_data, cinvite_data);

		// register callbacks
		connHandler->setTestingConnectivityCallback([invite_id_ref, jCallback_ref](const Remote::ConnectionHandlerRakNetClient* handler, bool result) {
			__android_log_print(ANDROID_LOG_DEBUG, "Nes", "connectivity test's on result handler invoked\n");

			InvokeClientConnectivityResultCallback(invite_id_ref->c_str(), handler->getRemoteGUID(), jCallback_ref->get(), result);
		});

		return reinterpret_cast<jlong>(connHandler);
	}

	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_startRemoteInternetConnectivityTestNative(JNIEnv *env, jobject thiz, jlong clientTestHandlerPtr) {
		auto connHandler = reinterpret_cast<Remote::ConnectionHandlerRakNetClient*>(clientTestHandlerPtr);
		if (connHandler) {
			auto started = connHandler->start();

			if (started) {
				return JNI_TRUE;
			}
		}

		return JNI_FALSE;
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_stopRemoteInternetConnectivityTestNative(JNIEnv *env, jobject thiz, jlong clientTestHandlerPtr) {
		auto clientTestHandler = reinterpret_cast<Remote::ConnectionHandlerRakNetClient*>(clientTestHandlerPtr);
		if (clientTestHandler) {
			clientTestHandler->stop();
			delete clientTestHandler;
		}
	}

	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_isRemoteConntectionViaProxyNative(JNIEnv *env, jobject thiz, jlong nativePtr) {
		auto system = (NesSystem *) nativePtr;

		bool usedProxy = false;
		auto connHandler = system->GetRemoteControllerClientConnHandler();
		if (connHandler && connHandler->getTag() == CONN_HANDLER_TYPE_INTERNET_CLIENT) {
			auto connHandlerClient = static_cast<Remote::ConnectionHandlerRakNetClient*>(connHandler.get());
			usedProxy = connHandlerClient->isConnectedThroughProxy();
		}

		return usedProxy ? JNI_TRUE : JNI_FALSE;
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_invokeNativeFunction(JNIEnv *env, jobject thiz, jlong nativePtr, jlong nativeFuncPtr, jlong nativeFuncArgs)
	{
		auto function = (async_func2_t)nativeFuncPtr;
		void* args = (void*)nativeFuncArgs;
		
		function(nativePtr, args);
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_enableRemoteControllerNative(JNIEnv *env, jobject thiz, jlong nativePtr, jint port, jstring hostName, jstring hostIpBound)
	{
		auto system = (NesSystem*)nativePtr;
		const char* chostName = hostName != NULL ? env->GetStringUTFChars(hostName, NULL) : NULL;
		const char* chostIpBound = hostIpBound != NULL ? env->GetStringUTFChars(hostIpBound, NULL) : NULL;
		
		//load
		std::shared_ptr<HQRemote::SocketServerHandler> connHandler = nullptr;
		if (chostIpBound)
			connHandler = std::make_shared<HQRemote::SocketServerHandler>(chostIpBound, port, port + 1);
		else
			connHandler = std::make_shared<HQRemote::SocketServerHandler>(port, port + 1);

		auto re = system->EnableRemoteController(1, connHandler, chostName);

		connHandler->setTag(CONN_HANDLER_TYPE_LAN_SERVER);
		
		if (chostName)
			env->ReleaseStringUTFChars(hostName, chostName);
		if (chostIpBound)
			env->ReleaseStringUTFChars(hostIpBound, chostIpBound);
		
		if (re)
			system->EnableAudioInput(true);
		
		return re;
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_enableRemoteControllerInternetNative(JNIEnv *env, jobject thiz, jlong nativePtr, jstring hostGUID, jstring hostName, jint context, jboolean isPublic)
	{
		auto system = (NesSystem*)nativePtr;
		
		auto chostGUID = guidFromJString(env, hostGUID);

		Remote::ConnectionHandlerRakNetServer::PublicServerMetaDataGenerator publicServerMetaDataGenerator = nullptr;
		if (isPublic) {
			// this server will be public.
			// use host name as public room's name
			const char* cpublicName = hostName ? env->GetStringUTFChars(hostName, NULL) : "A player";
			auto publicNameRef = std::make_shared<std::string>(cpublicName);
			if (hostName)
				env->ReleaseStringUTFChars(hostName, cpublicName);

			// need to generate public server's meta data, only java side needs it, so let it creates it for us.
			// The meta data will be stored on cloud lobby server. Java side can query later using http request.
			publicServerMetaDataGenerator = [context, publicNameRef] (const Remote::ConnectionHandlerRakNetServer* hostHandler) -> std::string {
				char serverInfo[256];
				if (!InitInternetServerConnectionInfo(hostHandler->getGUID(), hostHandler->getInvitationKey(), hostHandler->getLanPort(), serverInfo, sizeof(serverInfo)))
					return "";

				return CreatePublicServerMetaData(publicNameRef->c_str(), serverInfo, context);
			};
		}
		
		auto connHandler = std::make_shared<Remote::ConnectionHandlerRakNetServer>(&chostGUID,
																				   NAT_SERVER, NAT_SERVER_PORT,
																				   PROXY_SERVER, PROXY_SERVER_PORT,
																				   publicServerMetaDataGenerator,
																				   [context](const Remote::ConnectionHandlerRakNet* handler)
																				   {
																					   //successfully connected to central server
																					   auto hostHandler = static_cast<const Remote::ConnectionHandlerRakNetServer*>(handler);
																					   
																					   DisplayInviteDialog(hostHandler->getGUID(), hostHandler->getInvitationKey(), hostHandler->getLanPort(), context);
																				   });
		
		connHandler->setTag(CONN_HANDLER_TYPE_INTERNET_SERVER);
		
		//convert host name
		const char* chostName = hostName != NULL ? env->GetStringUTFChars(hostName, NULL) : NULL;
		
		//load
		auto re = system->EnableRemoteController(1, connHandler, chostName);
		
		if (chostName)
			env->ReleaseStringUTFChars(hostName, chostName);
		
		if (re)
			system->EnableAudioInput(true);
		
		return re;
	}

	JNIEXPORT jstring JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_getLobbyServerAddressNative(JNIEnv *env, jobject thiz) {
		return env->NewStringUTF(NAT_SERVER);
	}

	JNIEXPORT jstring JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_getLobbyAppIdNative(JNIEnv *env, jobject thiz) {
		char buf[64];
		sprintf(buf, "%" PRIu64, Remote::ConnectionHandlerRakNet::getIdForThisApp());
		return env->NewStringUTF(buf);
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_reinviteNewFriendForRemoteControllerInternetNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		
		auto connHandler = system->GetRemoteControllerHostConnHandler();
		if (connHandler->getTag() != CONN_HANDLER_TYPE_INTERNET_SERVER)
			return false;
		auto hostConnHandler = static_cast<Remote::ConnectionHandlerRakNetServer*>(connHandler.get());
		
		hostConnHandler->createNewInvitation();
			
		return true;
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_disableRemoteControllerNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		system->DisableRemoteController(1);
		system->EnableAudioInput(false);
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_isRemoteControllerEnabledNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		return system->RemoteControllerEnabled(1);
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_isRemoteControllerConnectedNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		return system->RemoteControllerConnected(1);
	}
	
	JNIEXPORT jint JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_loadStateNative(JNIEnv *env, jobject thiz, jlong nativePtr, jstring file){
		auto system = (NesSystem*)nativePtr;
		const char* cfile = env->GetStringUTFChars(file, NULL);
		auto re = system->LoadState(cfile);
		env->ReleaseStringUTFChars(file, cfile);
		
		return (jint)re;
	}
	
	JNIEXPORT jint JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_saveStateNative(JNIEnv *env, jobject thiz, jlong nativePtr, jstring file){
		auto system = (NesSystem*)nativePtr;
		const char* cfile = env->GetStringUTFChars(file, NULL);
		auto re = system->SaveState(cfile);
		env->ReleaseStringUTFChars(file, cfile);
		
		return (jint)re;
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_resetGameNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		system->Reset();
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_shutdownGameNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		
		system->EnableAudioInput(false);
		system->Shutdown();
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_setAudioVolumeNative(JNIEnv *env, jobject thiz, jlong nativePtr, jfloat gain)
	{
		auto system = (NesSystem*)nativePtr;
		system->SetAudioOutputVolume(gain);
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_setAudioRecordPermissionChangedNative(JNIEnv *env, jobject thiz, jlong nativePtr, jboolean has)
	{
		auto system = (NesSystem*)nativePtr;
		system->AudioRecordPermissionChanged(has);
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_enableAudioInput(JNIEnv *env, jobject thiz, jlong nativePtr, jboolean enable)
	{
		auto system = (NesSystem*)nativePtr;
		system->EnableAudioInput(enable && (system->RemoteControlling() || system->RemoteControllerEnabled(1)));
	}
	
	static Callback::OpenFileCallback GameSurfaceView_resourceLoaderFromJava(JNIEnv *env, jobject jassetManager)
	{
		return [] (const char* file) -> HQDataReaderStream* {
			if (!g_getAssetsMethodID || !g_BaseActivityClass)
				return nullptr;

			auto env = Jni::GetCurrenThreadJEnv();

			jobject jassetManager = env->CallStaticObjectMethod(g_BaseActivityClass, g_getAssetsMethodID);
			if (!jassetManager)
				return nullptr;

			auto assetManager = AAssetManager_fromJava(env, jassetManager);

			return new HQAssetDataReaderAndroid(assetManager, file);
		};
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_enableUIButtonsNative(JNIEnv *env, jobject thiz, jlong nativePtr, jboolean enable)
	{
		__android_log_print(ANDROID_LOG_DEBUG, "Nes", "enableUIButtonsNative(%d)\n", (int)enable);
		auto system = (NesSystem*)nativePtr;
		system->EnableUIButtons(enable);
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_setUIButtonRectNative(JNIEnv *env, jobject thiz, jlong nativePtr, jint buttonCode, float x, float y, float width, float height)
	{
		auto system = (NesSystem*)nativePtr;

		Maths::Rect rect;
		rect.x = x;
		rect.y = y;
		rect.width = width;
		rect.height = height;

		bool forPortrait = system->GetScreenWidth() < system->GetScreenHeight();

		system->GetInput().SetButtonRect((Input::Button::Id)buttonCode, rect, forPortrait);
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_setDPadRectNative(JNIEnv *env, jobject thiz, jlong nativePtr, float x, float y, float size)
	{
		auto system = (NesSystem*)nativePtr;

		bool forPortrait = system->GetScreenWidth() < system->GetScreenHeight();

		system->GetInput().SetDPadRect(x, y, size, forPortrait);
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_getUIButtonDefaultRectNative(JNIEnv *env, jobject thiz, jlong nativePtr, jint buttonCode, jfloatArray jvalues)
	{
		auto system = (NesSystem*)nativePtr;

		auto rect = Input::GetDefaultRect((Input::Button::Id)buttonCode, system->GetScreenWidth(), system->GetScreenHeight(), system->GetRenderer().GetVideoMinY());

		auto values = env->GetFloatArrayElements(jvalues, 0);

		values[0] = rect.x;
		values[1] = rect.y;
		values[2] = rect.width;
		values[3] = rect.height;

		env->ReleaseFloatArrayElements(jvalues, values, 0);
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_getDPadDefaultRectNative(JNIEnv *env, jobject thiz, jlong nativePtr, jfloatArray jvalues)
	{
		auto system = (NesSystem*)nativePtr;

		auto rect = Input::GetDefaultDpadRect(system->GetScreenWidth(), system->GetScreenHeight(), system->GetRenderer().GetVideoMinY());

		auto values = env->GetFloatArrayElements(jvalues, 0);

		values[0] = rect.x;
		values[1] = rect.y;
		values[2] = rect.width;

		env->ReleaseFloatArrayElements(jvalues, values, 0);
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_enableFullScreenNative(JNIEnv *env, jobject thiz, jlong nativePtr, jboolean enable)
	{
		__android_log_print(ANDROID_LOG_DEBUG, "Nes", "enableFullScreenNative(%d)\n", (int)enable);
		auto system = (NesSystem*)nativePtr;
		system->EnableFullScreenVideo(enable);
	}

	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_switchABTurboModeNative(JNIEnv *env, jobject thiz, jlong nativePtr, jboolean enableATurbo, jboolean enableBTurbo)
	{
		__android_log_print(ANDROID_LOG_DEBUG, "Nes", "switchABTurboModeNative(%d, %d)\n", (int)enableATurbo, (int)enableBTurbo);
		auto system = (NesSystem*)nativePtr;
		system->SwitchABTurboMode(enableATurbo, enableBTurbo);
	}

	JNIEXPORT jint JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_sendMessageToRemoteNative(JNIEnv *env, jobject thiz, jlong nativePtr, jlong id, jstring message)
	{
		auto system = (NesSystem*)nativePtr;
		
		const char* cmessage = env->GetStringUTFChars(message, NULL);
		
		auto re = system->SendMessageToRemote((uint64_t)id, cmessage);
		
		env->ReleaseStringUTFChars(message, cmessage);
		
		return (jint)re;
	}
	
	JNIEXPORT jstring JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_getRemoteNameNative(JNIEnv *env, jobject thiz, jlong nativePtr) {
		auto system = (NesSystem*)nativePtr;
		
		uint remoteIdx = system->RemoteControllerEnabled(1) ? 1 : -1;
		auto cname = system->GetRemoteName(remoteIdx);
		
		if (cname)
			return env->NewStringUTF(cname);
		return NULL;
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_resetGameViewNative(JNIEnv *env, jobject thiz, jobject jassetManager, jlong nativePtr, jint esVersionMajor, jint width, jint height, jboolean contextRecreate)
	{
		auto system = (NesSystem*)nativePtr;
		
		system->SetESVersion(esVersionMajor);
		
		system->ResetView(
						  (unsigned int)width, (unsigned int)height, contextRecreate,
						  GameSurfaceView_resourceLoaderFromJava(env, jassetManager)
						);
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_cleanupGameViewNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		
		system->CleanupGraphicsResources();
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_renderGameViewNative(JNIEnv *env, jobject thiz, jlong nativePtr, jboolean drawButtonOnly, jfloat buttonBoundingBoxOutlineSize)
	{
		auto system = (NesSystem*)nativePtr;

		if (drawButtonOnly) {
			glEnable(GL_BLEND);

			auto& inputManager = system->GetInput();
			auto inputDrawDisabled = !inputManager.IsUIEnabled();
			inputManager.EnableUI(true); // temporarily enable UI buttons so that they will be visible

			inputManager.SetBoundingBoxOutlineSize(buttonBoundingBoxOutlineSize);
			inputManager.BeginFrame();
			inputManager.EndFrame(system->GetRenderer(), true);

			inputManager.EnableUI(inputDrawDisabled);

		} else {
			// TODO: error callback
			system->Execute();
		}
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onPauseOnUIThreadNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		
		system->OnPause();
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onResumeNative(JNIEnv *env, jobject thiz, jlong nativePtr)
	{
		auto system = (NesSystem*)nativePtr;
		
		system->OnResume();
	}
	
	//LAN server discovery
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_enableLANServerDiscoveryNative(JNIEnv *env, jclass thiz, jlong nativePtr, jboolean enable)
	{
		auto system = (NesSystem*)nativePtr;
		
		if (enable)
			system->StartLanServerDiscoveryService();
		else
			system->StopLanServerDiscoveryService();
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_findLANServersNative(JNIEnv *env, jclass thiz, jlong nativePtr, jlong request_id)
	{
		auto system = (NesSystem*)nativePtr;
		
		system->FindLanServers((uint64_t)request_id);
	}
	
	//hardware input (thread safe)
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onJoystickMovedNative(JNIEnv *env, jobject thiz, jlong nativePtr, float x, float y) // x, y must be in range [-1, 1]
	{
		auto system = (NesSystem*)nativePtr;
		system->OnJoystickMoved(x, y);
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onUserHardwareButtonDownNative(JNIEnv *env, jobject thiz, jlong nativePtr, int buttonCode)
	{
		auto system = (NesSystem*)nativePtr;
		system->OnUserHardwareButtonDown(buttonCode);
	}
	
	JNIEXPORT void JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onUserHardwareButtonUpNative(JNIEnv *env, jobject thiz, jlong nativePtr, int buttonCode)
	{
		auto system = (NesSystem*)nativePtr;
		system->OnUserHardwareButtonUp(buttonCode);
	}
	
	//touch input
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onTouchBeganNative(JNIEnv *env, jobject thiz, jlong nativePtr, jint id, float x, float y)
	{
		auto system = (NesSystem*)nativePtr;
		return system->OnTouchBegan((void*)id, x, y);
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onTouchMovedNative(JNIEnv *env, jobject thiz, jlong nativePtr, jint id, float x, float y)
	{
		auto system = (NesSystem*)nativePtr;
		return system->OnTouchMoved((void*)id, x, y);
	}
	
	JNIEXPORT jboolean JNICALL
	Java_com_hqgame_networknes_GameSurfaceView_onTouchEndedNative(JNIEnv *env, jobject thiz, jlong nativePtr, jint id, float x, float y)
	{
		auto system = (NesSystem*)nativePtr;
		return system->OnTouchEnded((void*)id, x, y);
	}
}
