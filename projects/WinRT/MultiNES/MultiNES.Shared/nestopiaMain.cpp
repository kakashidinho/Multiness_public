#include "pch.h"
#include "nestopiaMain.h"
#include "Common\DirectXHelper.h"


#include <core/api/NstApiMemoryStream.hpp>
#include <mobile/winrt/NesSystemWrapperWinRT.hpp>
#include <mobile/winrt/WinRTUtils.hpp>
#include <mobile/winrt/InputD3D11.hpp>

#include <fstream>
#include <codecvt>
#include <string>

#include "RemoteController/Timer.h"
#include "RemoteController/ConnectionHandler.h"

#define DEFAULT_FRAME_RATE 60

using namespace MultiNES;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;
using namespace DirectX;

using namespace Nes;

static std::shared_ptr<NesSystemWrapperWinRT> g_nes;

// Loads and initializes application assets when the application is loaded.
nestopiaMain::nestopiaMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources),
	m_lastLoadRemoteTask(nullptr)
{
	// Register to be notified if the Device is lost or recreated
	m_deviceResources->RegisterDeviceNotify(this);

	// TODO: Change the timer settings if you want something other than the default variable timestep mode.
	// e.g. for 60 FPS fixed timestep update logic, call:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

nestopiaMain::~nestopiaMain()
{
	// Deregister device notification
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Updates application state when the window size changes (e.g. device orientation change)
void nestopiaMain::CreateWindowSizeDependentResources() 
{
	ResetNesView(false);
}

void nestopiaMain::LoadAndStartGame(Windows::Storage::StorageFile ^file)
{
	m_lastLoadRemoteTask = nullptr;

	QueueTaskOnRenderThread([this, file] {

		bool success = false;

		try {
			WinRT::Utils::StorageFileBuf fileBuf(file);

			std::istream fs(&fileBuf);

			std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_converter;
			auto filePathStr = utf8_converter.to_bytes(file->Path->Data());

			HQRemote::Log("Loading from %s", filePathStr);

			auto re = g_nes->LoadGame(filePathStr, fs);

			success = NES_SUCCEEDED(re);

			if (success)
			{
				//set fixed frame rate based on PAL or NTSC mode
				auto frameRate = g_nes->GetMode() == Api::Machine::PAL ? (DEFAULT_FRAME_RATE * 5 / 6) : DEFAULT_FRAME_RATE;
				m_timer.SetFixedTimeStep(true);
				m_timer.SetTargetElapsedSeconds(1.0 / frameRate);
			}
			else
				HQRemote::Log("Loading from %s failed re=%d!!!", filePathStr, (int)re);
		}
		catch (...) {
		}

		//error is handled by MachineEventHandler
	});
}

void nestopiaMain::LoadRemote(Platform::String^ ip, int port, Platform::String^ myName) {
	QueueTaskOnRenderThread(m_lastLoadRemoteTask = [this, ip, port, myName] {
		//convert to UTF8 strings
		std::string ipUTF8, myNameUtf8;
		std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_converter;

		ipUTF8 = utf8_converter.to_bytes(ip->Data());

		if (myName)
		{
			myNameUtf8 = utf8_converter.to_bytes(myName->Data());
		}
		else
			myNameUtf8 = "<No-name>";

		auto re = g_nes->LoadRemote(ipUTF8, port, myNameUtf8.c_str());

		//error is handled by MachineEventHandler
	});
}

void nestopiaMain::ReloadRemote() {
	//re-execute loading remote task
	if (m_lastLoadRemoteTask)
		QueueTaskOnRenderThread(m_lastLoadRemoteTask);
}

void nestopiaMain::EnableLanRemoteController(Platform::String^ hostName, int port) {
	QueueTaskOnRenderThread([this, hostName, port] {
		//convert to UTF8 string
		std::string hostNameUtf8;

		if (hostName)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_converter;
			hostNameUtf8 = utf8_converter.to_bytes(hostName->Data());
		}
		else
			hostNameUtf8 = "<No-name>";

		auto re = g_nes->EnableRemoteController(1, port, hostNameUtf8.c_str());

		//error is handled by MachineEventHandler
	});
}

void nestopiaMain::DisableRemoteController() {
	QueueTaskOnRenderThread([this] {
		g_nes->DisableRemoteController(1);
	});
}

void nestopiaMain::StartRenderLoop()
{
	auto uiDispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;

	// If the animation render loop is already running then do not start another thread.
	if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
	{
		return;
	}

	// Create a task that will be run on a background thread.
	auto workItemHandler = ref new WorkItemHandler([this, uiDispatcher](IAsyncAction ^ action)
	{
		CreateNesSystemOnce();

		{
			critical_section::scoped_lock lock(m_criticalSection);
			InitNesSystem();
		}

		g_nes->SetUIDispatcher(uiDispatcher);

		auto timerTickCallback = [&] {
			critical_section::scoped_lock lock(m_criticalSection);
			//run queued tasks
			ExecuteRenderLoopTasks(m_renderLoopTasks, false);

			//render
			Render();
		};

		//enable audio output
		g_nes->EnableAudioOutput(true);

		// Calculate the updated frame and render once per vertical blanking interval.
		while (action->Status == AsyncStatus::Started)
		{
			m_timer.Tick(timerTickCallback);
		}

		//run remaining queued tasks
		{
			critical_section::scoped_lock lock(m_criticalSection);

			ExecuteRenderLoopTasks(m_renderLoopTasks, false);
			ExecuteRenderLoopTasks(m_renderLoopEndingTasks, false);
		}

		//disable autdio output
		g_nes->EnableAudioOutput(false);
	});

	// Run task on a dedicated high priority background thread.
	m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void nestopiaMain::StopRenderLoop(bool exitGame)
{
	auto cleanupTask = [this] {
		if (!g_nes)
			return;
		g_nes->Shutdown();
		g_nes->CleanupGraphicsResources();
		//disable remote controller
		g_nes->DisableRemoteController(1);
	};

	if (m_renderLoopWorker && m_renderLoopWorker->Status == AsyncStatus::Started)
	{
		if (exitGame)
			QueueTaskOnRenderThread(m_renderLoopEndingTasks, cleanupTask);

		m_renderLoopWorker->Cancel();
	}
}

void nestopiaMain::ExecuteRenderLoopTasks(std::list<std::function<void()>> &tasks, bool lock)
{
	if (lock)
		m_criticalSection.lock();

	{
		critical_section::scoped_lock sl(m_tasksQueueLock);

		for (auto& task : tasks)
			task();

		tasks.clear();
	}

	if (lock)
		m_criticalSection.unlock();
}

void nestopiaMain::QueueTaskOnRenderThread(std::function<void()> task) {
	QueueTaskOnRenderThread(m_renderLoopTasks, task);
}

void nestopiaMain::QueueTaskOnRenderThread(std::list<std::function<void()>> &tasks, std::function<void()> task, bool top_priority) {
	critical_section::scoped_lock lock(m_tasksQueueLock);

	if (top_priority)
		tasks.push_front(task);
	else
		tasks.push_back(task);
}

// Renders the current frame according to the current application state.
void nestopiaMain::Render() 
{
	// Don't try to render anything before the first Update.
	if (m_timer.GetFrameCount() == 0)
	{
		return;
	}

	//intialize scene
	{
		auto context = m_deviceResources->GetD3DDeviceContext();

		// Reset the viewport to target the whole screen.
		auto viewport = m_deviceResources->GetScreenViewport();
		context->RSSetViewports(1, &viewport);

		// Reset render targets to the screen.
		ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
		context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

		// Clear the back buffer and depth stencil view.
		context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::Black);
		context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	// Render the scene.
	g_nes->Execute();

	// present the scene
	{
		m_deviceResources->Present();
	}
}

// Notifies renderers that device resources need to be released.
void nestopiaMain::OnDeviceLost()
{
	if (g_nes)
		g_nes->CleanupGraphicsResources();
}

// Notifies renderers that device resources may now be recreated.
void nestopiaMain::OnDeviceRestored()
{
	ResetNesView(true);

	CreateWindowSizeDependentResources();
}


void nestopiaMain::CreateNesSystemOnce() {
	//initialize nes system once
	m_criticalSection.lock();
	if (!g_nes) {
		m_criticalSection.unlock();//release lock to allow UI thread to do more works

		std::istream *dbStream = NULL;
		std::vector<unsigned char> dbData;

		if (WinRT::Utils::ReadResourceFile("NesDatabase.xml", dbData))
		{
			try {
				dbStream = new Api::MemInputStream((const char*)dbData.data(), dbData.size());
			}
			catch (...)
			{
				dbStream = NULL;
			}
		}

		auto nes = std::make_shared<NesSystemWrapperWinRT>(dbStream);

		delete dbStream;

		ResetNesView(true, true);

		m_criticalSection.lock();//lock again
		if (!g_nes)//check if NES system pointer hasn't been initialized by threads
			g_nes = nes;
	}
	m_criticalSection.unlock();
}

void nestopiaMain::InitNesSystem() {
	//register callbacks
	g_nes->SetMachineEventCallback([this](Api::Machine::Event event, Result result) {
		if (m_machineEventHander)
			m_machineEventHander->Invoke(event, result);
	});

	g_nes->SetLogCallback([](const char* format, va_list args) {
		HQRemote::LogV(format, args);
	});
}

void nestopiaMain::ResetNesView(bool d3dDeviceRecreated, bool top_priority) {
	auto resetTask = [this, d3dDeviceRecreated] {
		if (!g_nes)
			return;

		auto resLoader = [](const char* file) {
			return WinRT::Utils::LoadResourceFile(file);
		};

		g_nes->ResetD3D(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext(), m_deviceResources->GetOrientationTransform3D());
		g_nes->ResetView(m_deviceResources->GetOutputSize().Width, m_deviceResources->GetOutputSize().Height, d3dDeviceRecreated, resLoader);
	};

	QueueTaskOnRenderThread(m_renderLoopTasks, resetTask, top_priority);
}

/*---------- touch input -----------*/
Windows::Foundation::Point nestopiaMain::ConvertToNesScreen(float X, float Y) {
	auto windowBounds = m_deviceResources->GetLogicalSize();
	auto rotation = m_deviceResources->GetOrientationTransform3D();

	//our NES wrapper's coordinate originates at lower left corner
	XMFLOAT2 rotatedPoint(X, windowBounds.Height - Y);

	return Point(DX::ConvertDipsToPixels(rotatedPoint.x, m_deviceResources->GetDpi()),
				DX::ConvertDipsToPixels(rotatedPoint.y, m_deviceResources->GetDpi()));
}

void nestopiaMain::OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e) {
	if (g_nes)
	{
		auto pointer = e->CurrentPoint;
		auto point = ConvertToNesScreen(e->CurrentPoint->Position.X, e->CurrentPoint->Position.Y);

		g_nes->OnTouchBegan((void*)pointer->PointerId, point.X, point.Y);
	}
}

void nestopiaMain::OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e) {
	if (g_nes)
	{
		auto pointer = e->CurrentPoint;
		auto point = ConvertToNesScreen(e->CurrentPoint->Position.X, e->CurrentPoint->Position.Y);

		g_nes->OnTouchMoved((void*)pointer->PointerId, point.X, point.Y);
	}
}

void nestopiaMain::OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e) {
	if (g_nes)
	{
		auto pointer = e->CurrentPoint;
		auto point = ConvertToNesScreen(e->CurrentPoint->Position.X, e->CurrentPoint->Position.Y);

		g_nes->OnTouchEnded((void*)pointer->PointerId, point.X, point.Y);
	}
}