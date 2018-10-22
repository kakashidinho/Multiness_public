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

#ifndef INPUT_HPP
#define INPUT_HPP

#include "../core/api/NstApiInput.hpp"

#include "Renderer.hpp"
#include "Texture.hpp"

#include <set>
#include <mutex>

namespace Nes {
	namespace Input {
		/*----- UserHardwareButton -------*/
		enum class UserHardwareButton {
			LEFT, RIGHT, UP, DOWN,
			A,
			B,
			AUTO_A,
			AUTO_B,
			START,
			SELECT,
			AB,
			
			NUM_BUTTONS
		};
			
		/*------- IInput --------*/	
		class IInput: public Core::Input::Controllers {
		public:
			
			virtual ~IInput() {}
			
			void OnJoystickMoved(float x, float y); // x, y must be in range [-1, 1]
			void OnUserHardwareButtonDown(UserHardwareButton button);
			void OnUserHardwareButtonUp(UserHardwareButton button);
			
			bool OnTouchBegan(void* id, float x, float y);
			bool OnTouchMoved(void* id, float x, float y);
			bool OnTouchEnded(void* id, float x, float y);

			void Invalidate();//should be called when GL/DX context lost
			//renderer & resourceLoader are needed to reload dpad's resources after scaling
			void Reset(Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader);
			void ScaleDPad(float scale, Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader);
			void EnableUI(bool e);//enable/disable UI buttons
			// if enabled, normal A/B buttons will become auto A/B buttons
			void SwitchABTurboMode(bool aIsTurbo, bool bIsTurbo);

			void CleanupGraphicsResources();//cleanup graphics resources

			void BeginFrame();//this is called before NES engine's processing
			void EndFrame(Video::IRenderer& renderer);//this is called after NES engine's processing

		protected:
			class AutoFireState {
			public:
				AutoFireState();
				
				bool step();
				void start();
				void stop();
				void reset();
				bool isEnabled();
			private:
				int m_step;
				int m_signal;

				size_t m_numActivated;
			};


			virtual void SwitchABTurboModeImpl(bool aIsTurbo, bool bIsTurbo) = 0;
		
			virtual void OnJoystickMovedImpl(float x, float y) = 0;
			virtual void OnUserHardwareButtonDownImpl(UserHardwareButton button) = 0;
			virtual void OnUserHardwareButtonUpImpl(UserHardwareButton button) = 0;
		
			virtual bool OnTouchBeganImpl(void* id, float x, float y) = 0;
			virtual bool OnTouchMovedImpl(void* id, float x, float y) = 0;
			virtual bool OnTouchEndedImpl(void* id, float x, float y) = 0;

			virtual void InvalidateImpl() = 0;
			virtual void ResetImpl(Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) = 0;
			virtual void ScaleDPadImpl(float scale, Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) = 0;
			virtual void CleanupGraphicsResourcesImpl() = 0;

			virtual void BeginFrameImpl() = 0;//this is called before NES engine's processing
			virtual void EndFrameImpl(Video::IRectRenderer& renderer) = 0;

			std::mutex m_lock;
			bool m_uiEnabled;//enabled UI buttons?
		};
		
		
		class BaseInputPad: public IInput
		{
		public:
			BaseInputPad(bool deferResLoading);
			~BaseInputPad();

		protected:
			virtual void SwitchABTurboModeImpl(bool aIsTurbo, bool bIsTurbo) override;

			virtual void OnUserHardwareButtonDownImpl(UserHardwareButton button) override;
			virtual void OnUserHardwareButtonUpImpl(UserHardwareButton button) override;
			
			bool OnTouchDown(void* id, float x, float y);
			
			virtual bool OnTouchBeganImpl(void* id, float x, float y) override;
			virtual bool OnTouchMovedImpl(void* id, float x, float y) override;
			virtual bool OnTouchEndedImpl(void* id, float x, float y) override;

			virtual void InvalidateImpl() override;
			virtual void ResetImpl(Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) override;
			virtual void CleanupGraphicsResourcesImpl() override;

			virtual void BeginFrameImpl() override;
			virtual void EndFrameImpl(Video::IRectRenderer& renderer) override;
		
			Video::IStaticTexture* m_buttonTextures[5];
			Video::IStaticTexture* m_buttonHighlightTextures[5];
			Maths::Rect m_buttonRects[5];
			std::set<void*> m_buttonTouches[5];
			bool m_buttonPressed[5];
			bool m_hardwareButtonPressed[7];//hardware button pressed
			
			enum ABComboState: uint;
			
			ABComboState m_abComboState;
			
			AutoFireState m_autoAState;
			AutoFireState m_autoBState;
			bool m_aIsAuto; // normal A button will be auto?
			bool m_bIsAuto; // normal B button will be auto?

			Callback::OpenFileCallback m_resLoader;
			bool m_deferResLoading;
		};
		
		class InputDPad: public BaseInputPad
		{
		public:
			InputDPad(bool deferResLoading);
			~InputDPad();
		protected:
			virtual void OnJoystickMovedImpl(float x, float y) override;
			virtual void OnUserHardwareButtonDownImpl(UserHardwareButton button) override;
			virtual void OnUserHardwareButtonUpImpl(UserHardwareButton button) override;
			
			bool OnTouchDown(void* id, float x, float y, bool* pIsDpadTouch = nullptr);
			bool OnTouchDownEx(void* id, float x, float y, bool isJoystick, bool* pIsDpadTouch = nullptr);

			virtual bool OnTouchBeganImpl(void* id, float x, float y) override;
			virtual bool OnTouchMovedImpl(void* id, float x, float y) override;
			virtual bool OnTouchEndedImpl(void* id, float x, float y) override;

			virtual void InvalidateImpl() override;
			virtual void ResetImpl(Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) override;
			virtual void ScaleDPadImpl(float scale, Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) override;
			virtual void CleanupGraphicsResourcesImpl() override;

			virtual void BeginFrameImpl() override;
			virtual void EndFrameImpl(Video::IRectRenderer& renderer) override;
		
			Video::IStaticTexture* m_dPadTexture;
			Video::IStaticTexture* m_dpadDirectionHighlightTextures[4];
			
			Maths::Rect m_dPadRect;
			Maths::Range m_dPadDirectionAngleRegions[4];
			Maths::Rect m_dPadDirectionRects[4];
			Maths::Rect m_dPadDirectionOverlappedRects[4];
			
			//rectangle regions to draw highlight textures
			Maths::Rect m_dPadDirectionHighlightRects[4];
			
			std::set<void*> m_dPadTouches;
			std::set<void*> m_dPadDirectionTouches[4];
			bool m_hardwareDPadDirectionPressed[4];//hardware button pressed
			
			float m_dPadDeadzoneRadiusSq;//squared radius of d-pad's deadzone
			float m_dpadScale;
		};
	}
}

#endif