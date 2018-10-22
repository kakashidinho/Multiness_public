#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"

#include "core/api/NstApiEmulator.hpp"
#include "core/api/NstApiUser.hpp"
#include "core/api/NstApiVideo.hpp"
#include "core/api/NstApiSound.hpp"
#include "core/api/NstApiInput.hpp"
#include "core/api/NstApiMachine.hpp"

#include "RemoteController/ConnectionHandler.h"

#include <list>

// Renders Direct2D and 3D content on the screen.
namespace MultiNES
{
	delegate void NesMachineEventHandler(Nes::Api::Machine::Event event, Nes::Result result);

	class nestopiaMain : public DX::IDeviceNotify
	{
	public:
		nestopiaMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~nestopiaMain();
		void CreateWindowSizeDependentResources();
		void OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);

		void LoadAndStartGame(Windows::Storage::StorageFile^ file);
		void LoadRemote(Platform::String^ ip, int port, Platform::String^ myName);
		void ReloadRemote();
		void EnableLanRemoteController(Platform::String^ hostName, int port);
		void DisableRemoteController();

		void SetNesMachineEventHandler(NesMachineEventHandler^ handler) { m_machineEventHander = handler; }

		void QueueTaskOnRenderThread(std::function<void()> task);

		void StartRenderLoop();
		void StopRenderLoop(bool exitGame = false);

		Concurrency::critical_section& GetCriticalSection() { return m_criticalSection; }

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

	private:
		void QueueTaskOnRenderThread(std::list<std::function<void()>> &tasks, std::function<void()> task, bool top_priority = false);
		void ExecuteRenderLoopTasks(std::list<std::function<void()>> &tasks, bool lock);

		void Render();

		// NES
		void CreateNesSystemOnce();
		void InitNesSystem();
		void ResetNesView(bool d3dDeviceRecreated, bool top_priority = false);

		Windows::Foundation::Point ConvertToNesScreen(float X, float Y);

		// Cached pointer to device resources.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		Windows::Foundation::IAsyncAction^ m_renderLoopWorker;
		Concurrency::critical_section m_criticalSection;

		Concurrency::critical_section m_tasksQueueLock;

		std::list<std::function<void()>> m_renderLoopTasks;
		std::list<std::function<void()>> m_renderLoopEndingTasks;

		std::function<void()> m_lastLoadRemoteTask;

		// delegates
		NesMachineEventHandler^ m_machineEventHander;

		// Rendering loop timer.
		DX::StepTimer m_timer;
	};
}