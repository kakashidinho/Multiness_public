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

#include "Input.hpp"

#include "../core/NstLog.hpp"
#include <sstream>

#ifndef max
#	define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#	define min(a,b) ((a) < (b) ? (a) : (b))
#endif

namespace Nes {
	namespace Input {
		static const int DEFAULT_AUTOFIRE_SIGNAL = 6;
		static const int AUTOFIRE_STOP = -9999;

		static const int AUTO_A_IDX = Button::NUM_NORMAL_BUTTONS;
		static const int AUTO_B_IDX = Button::NUM_NORMAL_BUTTONS + 1;
		
		static const int UP_IDX = 0;
		static const int DOWN_IDX = 1;
		static const int LEFT_IDX = 2;
		static const int RIGHT_IDX = 3;
		
		static const uint buttonsCodes[] = {
			Core::Input::Controllers::Pad::A,
			Core::Input::Controllers::Pad::B,
			Core::Input::Controllers::Pad::SELECT,
			Core::Input::Controllers::Pad::START,
			0x80000000,
		};
		
		static const int DPadCodes[] = {
			Core::Input::Controllers::Pad::UP,
			Core::Input::Controllers::Pad::DOWN,
			Core::Input::Controllers::Pad::LEFT,
			Core::Input::Controllers::Pad::RIGHT,
		};
		
		static const char * buttonImages[] = {
			"A_button._png",
			"B_button._png",
			"select._png",
			"start._png",
			"AB_button._png",
		};
		
		static const char * buttonHighlightImages[] = {
			"A_button_highlight._png",
			"B_button_highlight._png",
			"select_highlight._png",
			"start_highlight._png",
			"AB_button_highlight._png",
		};

		static const char dpadImage[] = "dpad._png";
		
		static const char * dpadHighlightImages[] = {
			"dpad_up_highlight._png",
			"dpad_down_highlight._png",
			"dpad_left_highlight._png",
			"dpad_right_highlight._png",
		};
		
		enum BaseInputPad::ABComboState: uint {
			AB_COMBO_UNINITIALIZED,
			AB_COMBO_INITIALIZED,
			AB_COMBO_FINALIZING,
		};

		Maths::Rect GetDefaultRect(Button::Id button, float width, float height, float minVideoY) {

			Maths::Rect resultRect = {0};

			//calculate layout
			if (width <= height)
			{
				auto right_panel_size = width * 0.35f;
				auto middle_btn_width = width * 0.2f;
				auto middle_btn_height = middle_btn_width * 0.5f;
				auto middle_btn_gap = (right_panel_size - 2 * middle_btn_height) / 3.f;
				auto base_y = max(0.4f * (minVideoY - right_panel_size - 10), 10);
				auto a_btn_size = right_panel_size * 0.5f;
				auto a_btn_x = width - a_btn_size;
				auto b_btn_x = width - right_panel_size;

				switch (button) {
					case Button::SELECT:
					case Button::START: {

						resultRect.x = resultRect.x = (width - middle_btn_width) * 0.5f;
						resultRect.width = resultRect.width = middle_btn_width;
						resultRect.height = resultRect.height = middle_btn_height;

						auto select_rect_y = base_y + middle_btn_gap;

						if (button == Button::SELECT)
							resultRect.y = select_rect_y;
						else
							resultRect.y = select_rect_y + middle_btn_gap + middle_btn_height;
					}
						break;
					case Button::A:
					case Button::B: {
						resultRect.width = resultRect.height = a_btn_size;

						if (button == Button::B) {
							resultRect.x = b_btn_x;
							resultRect.y = base_y;
						} else {
							resultRect.x = a_btn_x;
							resultRect.y = base_y + right_panel_size * 0.5f;
						}
					}
						break;
					case Button::AB: {
						resultRect.width = resultRect.height = a_btn_size * 0.8f;
						auto abXGap = (a_btn_size - resultRect.width) * 0.8f;
						auto abYGap = (a_btn_size - resultRect.width) * 0.4f;
						resultRect.x = a_btn_x + abXGap;
						resultRect.y = base_y + abYGap;
					}
						break;
					default:
						// skip
						break;
				}
			}
			else {
				auto right_panel_size = min(width * 0.3f, height * 0.45f);
				auto middle_btn_width = width * 0.13f;
				auto middle_btn_height = middle_btn_width * 0.5f;
				auto select_btn_x = width * (0.5f - 0.015f) - middle_btn_width;
				auto a_btn_size = right_panel_size * 0.5f;
				auto b_btn_x = width - right_panel_size - 0.3f * (select_btn_x - right_panel_size);
				auto a_btn_x = b_btn_x + a_btn_size;
				auto a_btn_y = 10;

				switch (button) {
					case Button::SELECT:
					case Button::START: {
						resultRect.width = resultRect.width = middle_btn_width;
						resultRect.height = resultRect.height = middle_btn_height;
						resultRect.y = resultRect.y = 10;

						if (Button::SELECT == button) {
							resultRect.x = select_btn_x;
						} else {
							resultRect.x = width * (0.5f + 0.015f);
						}
					}
						break;
					case Button::A:
					case Button::B: {
						resultRect.width = resultRect.height = a_btn_size;
						resultRect.y = a_btn_y;

						if (button == Button::B) {
							resultRect.x = b_btn_x;
						} else {
							resultRect.x = a_btn_x;
						}
					}
						break;
					case Button::AB: {
						resultRect.width = resultRect.height = a_btn_size * 0.8f;
						auto abXGap = (a_btn_size - resultRect.width) * 0.8f;
						auto abYGap = (a_btn_size - resultRect.width) * 0.4f;
						resultRect.x = a_btn_x + abXGap;
						resultRect.y = a_btn_y + right_panel_size * 0.5f + abYGap;
					}
						break;
					default:
						// skip
						break;
				}
			}

			return resultRect;
		}

		Maths::Rect GetDefaultDpadRect(float width, float height, float minVideoY) {

			Maths::Rect dPadRect;

			auto selectRect = GetDefaultRect(Button::SELECT, width, height, minVideoY);

			if (width <= height) {
				dPadRect.width = dPadRect.height = width * 0.35f;
				dPadRect.x = 0;
				dPadRect.y = max(0.5f * (minVideoY - dPadRect.height), 10);
			}
			else {
				dPadRect.width = dPadRect.height = min(width * 0.3f, height * 0.45f) ;
				dPadRect.x = (selectRect.x - dPadRect.width) * 0.2f;
				dPadRect.y = 10;
			}

			return dPadRect;
		}

		/*-------------- IInput::AutoFireState ------------*/
		IInput::AutoFireState::AutoFireState()
		: m_signal(DEFAULT_AUTOFIRE_SIGNAL), m_step(AUTOFIRE_STOP),
		  m_numActivated(0)
		{}
				
		bool IInput::AutoFireState::step(){
			if (m_step == AUTOFIRE_STOP)
				return false;
			
			m_step = (m_step + 1) % m_signal;
			
			return m_step == 0;
		}
		void IInput::AutoFireState::start(){
			if (m_numActivated <= 0) {
				m_step = m_signal - 1;
				m_numActivated = 0;
			}

			m_numActivated ++;
		}
		
		void IInput::AutoFireState::stop(){
			if (m_numActivated > 0 && --m_numActivated == 0) {
				m_step = AUTOFIRE_STOP;
			}
		}

		void IInput::AutoFireState::reset(){
			m_numActivated = 0;
			m_step = AUTOFIRE_STOP;
		}

		bool IInput::AutoFireState::isEnabled() {
			return m_step != AUTOFIRE_STOP;
		}

		/*-------------- IInput ------------*/
		IInput::IInput()
		: m_currentDisplayIsPortrait(false), m_outlineSize(2.5f)
		{
			m_useCustomButtonsLayout[0] = m_useCustomButtonsLayout[1] = false;
		}

		void IInput::OnJoystickMoved(float x, float y) {
			std::lock_guard<std::mutex> lg(m_lock);

			return OnJoystickMovedImpl(x, y);
		}
		void IInput::OnUserHardwareButtonDown(UserHardwareButton button) {
			std::lock_guard<std::mutex> lg(m_lock);

			return OnUserHardwareButtonDownImpl(button);
		}
		void IInput::OnUserHardwareButtonUp(UserHardwareButton button) {
			std::lock_guard<std::mutex> lg(m_lock);

			return OnUserHardwareButtonUpImpl(button);
		}
			
		bool IInput::OnTouchBegan(void* id, float x, float y) {
			std::lock_guard<std::mutex> lg(m_lock);

			if (!m_uiEnabled)
				return false;
			
			return OnTouchBeganImpl(id, x, y);
		}

		bool IInput::OnTouchMoved(void* id, float x, float y) {
			std::lock_guard<std::mutex> lg(m_lock);
			
			if (!m_uiEnabled)
				return false;
			
			return OnTouchMovedImpl(id, x, y);
		}
		bool IInput::OnTouchEnded(void* id, float x, float y) {
			std::lock_guard<std::mutex> lg(m_lock);

			if (!m_uiEnabled)
				return false;
			
			return OnTouchEndedImpl(id, x, y);
		}

		void IInput::Invalidate()//should be called when GL/DX context lost
		{
			std::lock_guard<std::mutex> lg(m_lock);

			InvalidateImpl();
		}
		//renderer & resourceLoader are needed to reload dpad's resources after scaling
		void IInput::Reset(Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) {
			std::lock_guard<std::mutex> lg(m_lock);

			m_currentDisplayIsPortrait = renderer.GetScreenWidth() < renderer.GetScreenHeight();

			ResetImpl(renderer, resourceLoader);
		}
		
		void IInput::EnableUI(bool e) {
			std::lock_guard<std::mutex> lg(m_lock);
			
			m_uiEnabled = e;
		}

		void IInput::SwitchABTurboMode(bool aIsTurbo, bool bIsTurbo) {
			std::lock_guard<std::mutex> lg(m_lock);

			SwitchABTurboModeImpl(aIsTurbo, bIsTurbo);
		}

		void IInput::SetButtonRect(Button::Id button, const Maths::Rect& rect, bool forPortrait) {
			std::lock_guard<std::mutex> lg(m_lock);

			int isPortrait = forPortrait ? 1 : 0;
			m_useCustomButtonsLayout[isPortrait] = true;

			SetButtonRectImpl(button, rect, forPortrait);
		}

		void IInput::SetDPadRect(float x, float y, float size, bool forPortrait) {
			std::lock_guard<std::mutex> lg(m_lock);

			int isPortrait = forPortrait ? 1 : 0;
			m_useCustomButtonsLayout[isPortrait] = true;

			Maths::Rect rect;
			rect.x = x;
			rect.y = y;
			rect.width = rect.height = size;

			SetDPadRectImpl(rect, forPortrait);
		}

		void IInput::CleanupGraphicsResources()//cleanup graphics resources
		{
			std::lock_guard<std::mutex> lg(m_lock);

			CleanupGraphicsResourcesImpl();
		}

		void IInput::BeginFrame()//this is called before NES engine's processing
		{
			std::lock_guard<std::mutex> lg(m_lock);

			BeginFrameImpl();
		}
		void IInput::EndFrame(Video::IRenderer& renderer, bool drawBoundingBoxes)//this is called after NES engine's processing
		{
			std::lock_guard<std::mutex> lg(m_lock);

			if (m_uiEnabled) {
				EndFrameImpl(renderer);

				if (drawBoundingBoxes)
					DrawBoundingBoxesImpl(renderer);
			}
			else
			{
				//do not display the onscreen controls
				Video::NullRectRenderer nullRenderer;
				EndFrameImpl(nullRenderer);
			}
		}

		/*------------- BaseInputPad --------------------*/
		BaseInputPad::BaseInputPad(bool deferResLoading)
		: m_abComboState(AB_COMBO_UNINITIALIZED), m_deferResLoading(deferResLoading)
		{
			for (int i = 0; i < m_numButtons; ++i)
			{	
				m_buttonTextures[i] = nullptr;
				m_buttonHighlightTextures[i] = nullptr;
			}
		}
		BaseInputPad::~BaseInputPad() {
			for (int i = 0; i < m_numButtons; ++i)
			{	
				delete m_buttonTextures[i];
				delete m_buttonHighlightTextures[i];
			}
		}

		void BaseInputPad::SwitchABTurboModeImpl(bool aIsTurbo, bool bIsTurbo) {

			if (m_aIsAuto != aIsTurbo) {
				m_autoAState.reset();
				m_aIsAuto = aIsTurbo;
			}

			if (m_bIsAuto != bIsTurbo) {
				m_autoBState.reset();
				m_bIsAuto = bIsTurbo;
			}
		}
		
		void BaseInputPad::OnUserHardwareButtonDownImpl(UserHardwareButton button) {
			switch (button) {
				case UserHardwareButton::A:
					m_hardwareButtonPressed[Button::A] = true;
					break;
				case UserHardwareButton::B:
					m_hardwareButtonPressed[Button::B] = true;
					break;
				case UserHardwareButton::AB:
					m_hardwareButtonPressed[Button::AB] = true;
					break;
				case UserHardwareButton::SELECT:
					m_hardwareButtonPressed[Button::SELECT] = true;
					break;
				case UserHardwareButton::START:
					m_hardwareButtonPressed[Button::START] = true;
					break;
				case UserHardwareButton::AUTO_A:
					m_hardwareButtonPressed[AUTO_A_IDX] = true;
					if (!m_autoAState.isEnabled())
						m_autoAState.start();
					break;
				case UserHardwareButton::AUTO_B:
					m_hardwareButtonPressed[AUTO_B_IDX] = true;
					if (!m_autoBState.isEnabled())
						m_autoBState.start();
					break;
			}//switch (button)
		}
		
		void BaseInputPad::OnUserHardwareButtonUpImpl(UserHardwareButton button) {
			switch (button) {
				case UserHardwareButton::A:
					m_hardwareButtonPressed[Button::A] = false;
					break;
				case UserHardwareButton::B:
					m_hardwareButtonPressed[Button::B] = false;
					break;
				case UserHardwareButton::AB:
					m_hardwareButtonPressed[Button::AB] = false;
					break;
				case UserHardwareButton::SELECT:
					m_hardwareButtonPressed[Button::SELECT] = false;
					break;
				case UserHardwareButton::START:
					m_hardwareButtonPressed[Button::START] = false;
					break;
				case UserHardwareButton::AUTO_A:
					m_hardwareButtonPressed[AUTO_A_IDX] = false;
					if (m_autoAState.isEnabled())
						m_autoAState.stop();
					break;
				case UserHardwareButton::AUTO_B:
					m_hardwareButtonPressed[AUTO_B_IDX] = false;
					if (m_autoBState.isEnabled())
						m_autoBState.stop();
					break;
			}//switch (button)
		}
			
		bool BaseInputPad::OnTouchDown(void* id, float x, float y)
		{
			bool reacted = false;
			for (int i = 0; i < m_numButtons; ++i)
			{
				if (m_buttonRects[i].contains(x, y))
				{
					if (m_buttonTouches[i].find(id) == m_buttonTouches[i].end())
					{	
						m_buttonTouches[i].insert(id);
						
						reacted = true;
					}
				}
				else {
					auto ite = m_buttonTouches[i].find(id);
					if (ite != m_buttonTouches[i].end())
					{	
						m_buttonTouches[i].erase(ite);
						
						reacted = true;
					}
				}
			}
			
			return reacted;
		}		
			
		bool BaseInputPad::OnTouchBeganImpl(void* id, float x, float y)  {
			return OnTouchDown(id, x, y);
		}
		
		bool BaseInputPad::OnTouchMovedImpl(void* id, float x, float y)  {
			return OnTouchDown(id, x, y);
		}
		
		bool BaseInputPad::OnTouchEndedImpl(void* id, float x, float y)  {
			bool reacted = false;
			for (int i = 0; i < m_numButtons; ++i)
			{
				auto ite = m_buttonTouches[i].find(id);
				if (ite != m_buttonTouches[i].end())
				{	
					m_buttonTouches[i].erase(ite);
					
					reacted = true;
				}
			}
			
			return reacted;
		}
			
		void BaseInputPad::InvalidateImpl()
		{
			for (int i = 0; i < m_numButtons; ++i)
			{	
				m_buttonTextures[i]->Invalidate();
				m_buttonHighlightTextures[i]->Invalidate();
			}
		}

		void BaseInputPad::SetButtonRectImpl(Button::Id button, const Maths::Rect& rect, bool forPortrait) {
			int isPortrait = forPortrait ? 1 : 0;
			if (button >= m_numButtons)
				return;
			m_buttonRectsSettings[isPortrait][button] = rect;
		}
		
		void BaseInputPad::ResetImpl(Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) {
			auto width = renderer.GetScreenWidth();
			auto height = renderer.GetScreenHeight();
			unsigned int minVideoY = renderer.GetVideoMinY();

			int isPortrait = width < height ? 1 : 0;
			m_buttonRects = m_buttonRectsSettings[isPortrait];
			
			for (int i = 0; i < m_numButtons; ++i)
			{
				if (!m_useCustomButtonsLayout[isPortrait]) {
					m_buttonRects[i] = GetDefaultRect((Button::Id)i, width, height, minVideoY);
				}

				if (!m_deferResLoading)
				{
					//reload textures immediately
					m_buttonTextures[i]->ResetIfInvalid(buttonImages[i], resourceLoader);
					m_buttonHighlightTextures[i]->ResetIfInvalid(buttonHighlightImages[i], resourceLoader);
				}

				//reset touch tracking
				m_buttonTouches[i].clear();
			}

			if (m_deferResLoading)//cache resource loader for defer loading later
				m_resLoader = resourceLoader;
			
			//AB combo state
			m_abComboState = AB_COMBO_UNINITIALIZED;
			
			//auto buttons' states
			m_autoAState.reset();
			m_autoBState.reset();
			
			//user hardware buttons' pressed states
			for (int i = 0; i < sizeof(m_hardwareButtonPressed) / sizeof(m_hardwareButtonPressed[0]); ++i)
			{
				m_hardwareButtonPressed[i] = false;
			}
		}
		
		void BaseInputPad::CleanupGraphicsResourcesImpl() {
			//release textures
			for (int i = 0; i < m_numButtons; ++i)
			{
				m_buttonTextures[i]->Cleanup();
				m_buttonHighlightTextures[i]->Cleanup();
			}
		}
		
		void BaseInputPad::BeginFrameImpl() {
			this->pad[0].buttons = 0;
			
			for (int i = 0; i < m_numButtons; ++i)
			{
				if ((m_buttonPressed[i] = m_buttonTouches[i].size() > 0) || m_hardwareButtonPressed[i]) {
					if (i == Button::AB)//this is special combo button
					{
						//the AB combo button will generate the button pressed state in 3 phases: A pressed -> A & B pressed -> B pressed -> none
						switch (m_abComboState) {
							case AB_COMBO_UNINITIALIZED:
							case AB_COMBO_FINALIZING:
								m_abComboState = AB_COMBO_INITIALIZED;
								this->pad[0].buttons |= Core::Input::Controllers::Pad::A;
								break;
								
							case AB_COMBO_INITIALIZED:
								this->pad[0].buttons |= Core::Input::Controllers::Pad::A | Core::Input::Controllers::Pad::B;
								break;
								
						}
					}//if (i == Button::AB)
					else {
						// handle cases when user wants normal A/B button to act as auto A/B button
						if (i == Button::A && m_aIsAuto) {
							if (!m_autoAState.isEnabled())
								m_autoAState.start();
						} else if (i == Button::B && m_bIsAuto) {
							if (!m_autoBState.isEnabled())
								m_autoBState.start();
						} else {
						    this->pad[0].buttons |= buttonsCodes[i];
						}
					}
				}//if (m_buttonPressed[i])
				else {
					if (i == Button::AB)
					{
						switch (m_abComboState) {
							case AB_COMBO_INITIALIZED:
								m_abComboState = AB_COMBO_FINALIZING;
								this->pad[0].buttons |= Core::Input::Controllers::Pad::B;
								break;
							case AB_COMBO_FINALIZING:
								m_abComboState = AB_COMBO_UNINITIALIZED;
								break;
							default:
								break;
						}
					}//if (i == Button::AB)
					// handle cases when user wants normal A/B button to act as auto A/B button
					else if (i == Button::A && m_aIsAuto) {
						if (!m_hardwareButtonPressed[AUTO_A_IDX] && m_autoAState.isEnabled())
							m_autoAState.stop();
					} else if (i == Button::B && m_bIsAuto) {
						if (!m_hardwareButtonPressed[AUTO_B_IDX] && m_autoBState.isEnabled())
							m_autoBState.stop();
					}
				}
			}//for (int i = 0; i < m_numButtons; ++i)
				
			//autofire buttons' state update
			if (m_autoAState.step())
			{
				this->pad[0].buttons |= Core::Input::Controllers::Pad::A;
			}
			
			if (m_autoBState.step())
			{
				this->pad[0].buttons |= Core::Input::Controllers::Pad::B;
			}
		}
		
		void BaseInputPad::EndFrameImpl(Video::IRectRenderer& renderer)
		{
			for (int i = 0; i < m_numButtons; ++i)
			{
				//perform defer loading if the texture hasn't been loaded yet
				if (m_buttonTextures[i]->IsInvalid())
				{
					m_buttonTextures[i]->Reset(buttonImages[i], m_resLoader);
					break;//stop drawing process
				}
				else if (m_buttonHighlightTextures[i]->IsInvalid())
				{
					m_buttonHighlightTextures[i]->Reset(buttonHighlightImages[i], m_resLoader);
					break;//stop drawing process
				}


				//draw appropriate image based on button's state
				if (m_buttonPressed[i])
					renderer.DrawRect(*m_buttonHighlightTextures[i], m_buttonRects[i]);
				else
					renderer.DrawRect(*m_buttonTextures[i], m_buttonRects[i]);
			}
		}

		void BaseInputPad::DrawBoundingBoxesImpl(Video::IRectRenderer& renderer) {
			Video::Color color = {.r = 1, .g = 0, .b=0, .a=1};
			for (int i = 0; i < m_numButtons; ++i) {
				renderer.DrawOutlineRect(color, m_outlineSize, m_buttonRects[i]);
			}
		}
		
		/*-------------- InputDPad --------------*/
		InputDPad::InputDPad(bool deferResLoading)
		: BaseInputPad(deferResLoading), m_dPadTexture(nullptr), m_dpadScale(1.f)
		{
			for (int i = 0; i < sizeof(m_dpadDirectionHighlightTextures) / sizeof(m_dpadDirectionHighlightTextures[0]); ++i)
			{
				m_dpadDirectionHighlightTextures[i] = nullptr;
			}
		}
		InputDPad::~InputDPad() {
			delete m_dPadTexture;
			
			for (int i = 0; i < sizeof(m_dpadDirectionHighlightTextures) / sizeof(m_dpadDirectionHighlightTextures[0]); ++i)
			{
				delete m_dpadDirectionHighlightTextures[i];
			}
		}
		
		void InputDPad::OnJoystickMovedImpl(float x, float y) {
			//generate fake touch event inside the d-pad area
			if (x > 0)
				x -= 0.0001f;
			else
				x += 0.0001f;
			
			if (y > 0)
				y -= 0.0001f;
			else
				y += 0.0001f;
			
			float fake_touch_x = m_dPadRect.getMidX() + m_dPadRect.width * x * 0.5f;
			float fake_touch_y = m_dPadRect.getMidY() + m_dPadRect.height * y * 0.5f;
			
#if 0
			std::stringstream ss;
			
			ss << "(" << x << ", " << y << ") -> (" << fake_touch_x << ", " << fake_touch_y << ")";
			Core::Log() << ss.str().c_str();
#endif//debug log
			
			OnTouchDownEx(NULL, fake_touch_x, fake_touch_y, true, NULL);
		}
		
		void InputDPad::OnUserHardwareButtonDownImpl(UserHardwareButton button) {
			BaseInputPad::OnUserHardwareButtonDownImpl(button);
			
			switch (button) {
				case UserHardwareButton::LEFT:
					m_hardwareDPadDirectionPressed[LEFT_IDX] = true;
					break;
				case UserHardwareButton::RIGHT:
					m_hardwareDPadDirectionPressed[RIGHT_IDX] = true;
					break;
				case UserHardwareButton::UP:
					m_hardwareDPadDirectionPressed[UP_IDX] = true;
					break;
				case UserHardwareButton::DOWN:
					m_hardwareDPadDirectionPressed[DOWN_IDX] = true;
					break;
			}//switch (button)
		}
		
		void InputDPad::OnUserHardwareButtonUpImpl(UserHardwareButton button) {
			BaseInputPad::OnUserHardwareButtonUpImpl(button);
			
			switch (button) {
				case UserHardwareButton::LEFT:
					m_hardwareDPadDirectionPressed[LEFT_IDX] = false;
					break;
				case UserHardwareButton::RIGHT:
					m_hardwareDPadDirectionPressed[RIGHT_IDX] = false;
					break;
				case UserHardwareButton::UP:
					m_hardwareDPadDirectionPressed[UP_IDX] = false;
					break;
				case UserHardwareButton::DOWN:
					m_hardwareDPadDirectionPressed[DOWN_IDX] = false;
					break;
			}//switch (button)
		}
			
		bool InputDPad::OnTouchDown(void* id, float x, float y, bool* pIsDpadTouch)
		{
			return OnTouchDownEx(id, x, y, false, pIsDpadTouch);
		}
		
		bool InputDPad::OnTouchDownEx(void* id, float x, float y, bool isJoystick, bool* pIsDpadTouch)
		{
			if (!isJoystick)
			{
				//check if touch is inside dpad region or was inside before and hasn't been ended yet
				bool insideDpad = m_dPadRect.contains(x, y);
				auto touchIte = m_dPadTouches.find(id);
				if (touchIte == m_dPadTouches.end() && insideDpad)
					touchIte = m_dPadTouches.insert(id).first;
				
				if (touchIte == m_dPadTouches.end())
				{
					if (pIsDpadTouch)
						*pIsDpadTouch = false;
					return false;
				}
				if (pIsDpadTouch)//touch is inside dpad region or was inside before and hasn't been ended yet
					*pIsDpadTouch = true;
			}//if (!isJoystick)
			
			bool isInDeadzone = false;
			Maths::Vec2 touchPoint(x, y);
			auto distVecFromDpadCenter = m_dPadRect.distanceVecFromCenter(touchPoint);
			if (distVecFromDpadCenter.lengthSq() < m_dPadDeadzoneRadiusSq)
				isInDeadzone = true;//touch is inside deadzone
			
			bool reacted = false;
			
			auto vecFromDpadCenter = distVecFromDpadCenter.normalizedVec();
			auto angle = Maths::Vec2::XAXIS.angle(vecFromDpadCenter);
			
			//check if any direction button is touched
			for (int i = 0; i < sizeof(m_dPadDirectionTouches) / sizeof(m_dPadDirectionTouches[0]); ++i)
			{
				//either the touch is inside button' rectangle region or inside the buttons's angular region
				if (!isInDeadzone &&
					(m_dPadDirectionRects[i].contains(x, y) || m_dPadDirectionOverlappedRects[i].contains(x, y) ||
					 ((m_dPadDirectionAngleRegions[i].contains(angle) ||
					   m_dPadDirectionAngleRegions[i].contains(angle + 2 * Maths::_PI)
					  )
					 )
					)
					)
				{
					if (isJoystick)
					{
						m_hardwareDPadDirectionPressed[i] = true;
					}//if (isJoystick)
					else if (m_dPadDirectionTouches[i].find(id) == m_dPadDirectionTouches[i].end())
					{	
						m_dPadDirectionTouches[i].insert(id);
						
						reacted = true;//UI button feedback
					}//if (isJoystick)
				}
				else {
					if (isJoystick)
					{
						m_hardwareDPadDirectionPressed[i] = false;
					}//if (isJoystick)
					else {
						auto ite = m_dPadDirectionTouches[i].find(id);
						if (ite != m_dPadDirectionTouches[i].end())
						{	
							m_dPadDirectionTouches[i].erase(ite);
							
							reacted = true;//UI button feedback
						}
					}//if (isJoystick)
				}
			}
			
			return reacted;
		}		
			
		bool InputDPad::OnTouchBeganImpl(void* id, float x, float y)  {
			bool reacted = BaseInputPad::OnTouchBeganImpl(id, x, y);//base version
			
			return OnTouchDown(id, x, y) || reacted;//InputDPad's version
		}
		
		bool InputDPad::OnTouchMovedImpl(void* id, float x, float y)  {
			bool isDpadTouch;
			//check if this touch is controlling the dpad
			auto reacted = OnTouchDown(id, x, y, &isDpadTouch);
			
			//if this touch is not controlling the dpad, we check other buttons, otherwise, ignore them
			if (!isDpadTouch)
				reacted = BaseInputPad::OnTouchMovedImpl(id, x, y) || reacted;
			return reacted;
		}
		
		bool InputDPad::OnTouchEndedImpl(void* id, float x, float y)  {
			bool reacted = BaseInputPad::OnTouchEndedImpl(id, x, y);//base version
			
			auto ite = m_dPadTouches.find(id);
			if (ite != m_dPadTouches.end())
				m_dPadTouches.erase(ite);
				
			//check if any direction button was touched
			for (int i = 0; i < sizeof(m_dPadDirectionTouches) / sizeof(m_dPadDirectionTouches[0]); ++i)
			{
				auto ite2 = m_dPadDirectionTouches[i].find(id);
				if (ite2 != m_dPadDirectionTouches[i].end())
				{	
					m_dPadDirectionTouches[i].erase(ite2);
					
					reacted = true;
				}
			}
			
			return reacted;
		}
			
		void InputDPad::InvalidateImpl()
		{
			BaseInputPad::InvalidateImpl();
			
			m_dPadTexture->Invalidate();
			
			for (int i = 0; i < sizeof(m_dpadDirectionHighlightTextures) / sizeof(m_dpadDirectionHighlightTextures[0]); ++i)
			{
				m_dpadDirectionHighlightTextures[i]->Invalidate();
			}
		}

		void InputDPad::ResetDPadRelatedRegions() {
			// recalculate dpad-related info
			auto deadzoneRadius = m_dPadRect.width * 0.1f;
			m_dPadDeadzoneRadiusSq = deadzoneRadius * deadzoneRadius;

			const auto halfRange = Maths::_PI / 3.f;// * 3.f / 8.f;
			auto dpadDirectionWidth = m_dPadRect.width * 0.35f;
			auto dpadDirectionHeight = m_dPadRect.width * 0.5f;
			const auto dpadDirectionOverlappedSize = (m_dPadRect.width - dpadDirectionWidth) * 0.5f;

			//direction buttons' angular regions
			m_dPadDirectionAngleRegions[RIGHT_IDX].min = -halfRange;
			m_dPadDirectionAngleRegions[RIGHT_IDX].max = halfRange;

			m_dPadDirectionAngleRegions[LEFT_IDX].min = Maths::_PI - halfRange;
			m_dPadDirectionAngleRegions[LEFT_IDX].max = Maths::_PI + halfRange;

			m_dPadDirectionAngleRegions[UP_IDX].min = Maths::_PI / 2.f - halfRange;
			m_dPadDirectionAngleRegions[UP_IDX].max = Maths::_PI / 2.f + halfRange;

			m_dPadDirectionAngleRegions[DOWN_IDX].min = Maths::_PI * 1.5f - halfRange;
			m_dPadDirectionAngleRegions[DOWN_IDX].max = Maths::_PI * 1.5f + halfRange;

			//direction buttons's overlapped rectangle regions (to be used as diagonal direction detection)
			m_dPadDirectionOverlappedRects[RIGHT_IDX].x = m_dPadRect.x + m_dPadRect.width - dpadDirectionOverlappedSize;
			m_dPadDirectionOverlappedRects[LEFT_IDX].x = m_dPadRect.x;
			m_dPadDirectionOverlappedRects[RIGHT_IDX].y = m_dPadDirectionOverlappedRects[LEFT_IDX].y = m_dPadRect.y;
			m_dPadDirectionOverlappedRects[RIGHT_IDX].width = m_dPadDirectionOverlappedRects[LEFT_IDX].width = dpadDirectionOverlappedSize;
			m_dPadDirectionOverlappedRects[RIGHT_IDX].height = m_dPadDirectionOverlappedRects[LEFT_IDX].height = m_dPadRect.height;

			m_dPadDirectionOverlappedRects[UP_IDX].y = m_dPadRect.y + m_dPadRect.height - dpadDirectionOverlappedSize;
			m_dPadDirectionOverlappedRects[DOWN_IDX].y = m_dPadRect.y;
			m_dPadDirectionOverlappedRects[UP_IDX].x = m_dPadDirectionOverlappedRects[DOWN_IDX].x = m_dPadRect.x;
			m_dPadDirectionOverlappedRects[UP_IDX].height = m_dPadDirectionOverlappedRects[DOWN_IDX].height = dpadDirectionOverlappedSize;
			m_dPadDirectionOverlappedRects[UP_IDX].width = m_dPadDirectionOverlappedRects[DOWN_IDX].width = m_dPadRect.width;

			//directional buttons' rectangle regions
			m_dPadDirectionRects[UP_IDX].x = m_dPadDirectionRects[DOWN_IDX].x = m_dPadRect.x + (m_dPadRect.width - dpadDirectionWidth) * 0.5f;
			m_dPadDirectionRects[UP_IDX].width = m_dPadDirectionRects[DOWN_IDX].width = dpadDirectionWidth;
			m_dPadDirectionRects[UP_IDX].height = m_dPadDirectionRects[DOWN_IDX].height = dpadDirectionHeight;
			m_dPadDirectionRects[UP_IDX].y = m_dPadRect.y + m_dPadDirectionRects[DOWN_IDX].height;
			m_dPadDirectionRects[DOWN_IDX].y = m_dPadRect.y;

			m_dPadDirectionRects[LEFT_IDX].y = m_dPadDirectionRects[RIGHT_IDX].y = m_dPadRect.y + (m_dPadRect.height - dpadDirectionWidth) * 0.5f;
			m_dPadDirectionRects[LEFT_IDX].width = m_dPadDirectionRects[RIGHT_IDX].width = dpadDirectionHeight;
			m_dPadDirectionRects[LEFT_IDX].height = m_dPadDirectionRects[RIGHT_IDX].height = dpadDirectionWidth;
			m_dPadDirectionRects[LEFT_IDX].x = m_dPadRect.x;
			m_dPadDirectionRects[RIGHT_IDX].x = m_dPadRect.x + m_dPadDirectionRects[LEFT_IDX].width;

			//directional buttons' highlight textures' regions
			auto highlightRectSize = m_dPadRect.width * 0.5f;
			auto highlightRectOffset = m_dPadRect.width * 0.25f;
			m_dPadDirectionHighlightRects[UP_IDX].x = m_dPadDirectionHighlightRects[DOWN_IDX].x = m_dPadRect.x + highlightRectOffset;
			m_dPadDirectionHighlightRects[UP_IDX].y = m_dPadRect.y + highlightRectSize;
			m_dPadDirectionHighlightRects[DOWN_IDX].y = m_dPadRect.y;

			m_dPadDirectionHighlightRects[LEFT_IDX].y = m_dPadDirectionHighlightRects[RIGHT_IDX].y = m_dPadRect.y + highlightRectOffset;
			m_dPadDirectionHighlightRects[LEFT_IDX].x = m_dPadRect.x;
			m_dPadDirectionHighlightRects[RIGHT_IDX].x = m_dPadRect.x + highlightRectSize;

			m_dPadDirectionHighlightRects[UP_IDX].width = m_dPadDirectionHighlightRects[UP_IDX].height =
			m_dPadDirectionHighlightRects[DOWN_IDX].width = m_dPadDirectionHighlightRects[DOWN_IDX].height =
			m_dPadDirectionHighlightRects[LEFT_IDX].width = m_dPadDirectionHighlightRects[LEFT_IDX].height =
			m_dPadDirectionHighlightRects[RIGHT_IDX].width = m_dPadDirectionHighlightRects[RIGHT_IDX].height = highlightRectSize;
		}

		void InputDPad::ClearDPadTouchesTracking() {
			//reset whole dpad's touch tracking
			m_dPadTouches.clear();
			for (int i = 0; i < sizeof(m_dPadDirectionTouches) / sizeof(m_dPadDirectionTouches[0]); ++i)
			{
				//reset direction specific touch tracking
				m_dPadDirectionTouches[i].clear();

				//reset hardware buttons' state
				m_hardwareDPadDirectionPressed[i] = false;
			}
		}

		void InputDPad::SetDPadRectImpl(const Maths::Rect& rect, bool forPortrait) {
			int isPortrait = forPortrait ? 1 : 0;

			m_dPadRectSettings[isPortrait] = rect;

			if (m_currentDisplayIsPortrait == forPortrait) {
				// apply the change
				m_dPadRect = m_dPadRectSettings[isPortrait];

				ResetDPadRelatedRegions();
				ClearDPadTouchesTracking();
			}
		}
		
		void InputDPad::ResetImpl(Video::IRenderer& renderer, Callback::OpenFileCallback resourceLoader) {
			BaseInputPad::ResetImpl(renderer, resourceLoader);
			
			auto width = renderer.GetScreenWidth();
			auto height = renderer.GetScreenHeight();

			int isPortrait = width < height ? 1 : 0;

			//calculate d-pad layout
			if (!m_useCustomButtonsLayout[isPortrait]) {
				unsigned int minVideoY = renderer.GetVideoMinY();

				m_dPadRectSettings[isPortrait] = GetDefaultDpadRect(width, height, minVideoY);
			}

			m_dPadRect = m_dPadRectSettings[isPortrait];

			ResetDPadRelatedRegions();
			ClearDPadTouchesTracking();

			if (!m_deferResLoading) {
				// reload base texture
				m_dPadTexture->ResetIfInvalid(dpadImage, resourceLoader);

				// reset highlight textures
				for (int i = 0; i < sizeof(m_dPadDirectionTouches) / sizeof(m_dPadDirectionTouches[0]); ++i) {
					m_dpadDirectionHighlightTextures[i]->ResetIfInvalid(dpadHighlightImages[i], resourceLoader);
				}
			}
		}
		
		void InputDPad::CleanupGraphicsResourcesImpl() {
			BaseInputPad::CleanupGraphicsResourcesImpl();
			
			//release textures
			m_dPadTexture->Cleanup();
			
			for (int i = 0; i < sizeof(m_dPadDirectionTouches) / sizeof(m_dPadDirectionTouches[0]); ++i)
			{
				m_dpadDirectionHighlightTextures[i]->Cleanup();
			}
		}
		
		void InputDPad::BeginFrameImpl() {
			BaseInputPad::BeginFrameImpl();
			
			for (int i = 0; i < sizeof(m_dPadDirectionTouches) / sizeof(m_dPadDirectionTouches[0]); ++i)
			{
				if (m_dPadDirectionTouches[i].size() || m_hardwareDPadDirectionPressed[i])
					this->pad[0].buttons |= DPadCodes[i];
			}
		}
		
		void InputDPad::EndFrameImpl(Video::IRectRenderer& renderer)
		{
			BaseInputPad::EndFrameImpl(renderer);
			
			//perform defer loading if the texture hasn't been loaded yet
			if (m_dPadTexture->IsInvalid())
			{
				m_dPadTexture->Reset(dpadImage, m_resLoader);
				return;//stop drawing process
			}

			//draw d-pad image
			renderer.DrawRect(*m_dPadTexture, m_dPadRect);
			
			for (int i = 0; i < sizeof(m_dPadDirectionTouches) / sizeof(m_dPadDirectionTouches[0]); ++i)
			{
				//perform defer loading if the texture hasn't been loaded yet
				if (m_dpadDirectionHighlightTextures[i]->IsInvalid())
				{
					m_dpadDirectionHighlightTextures[i]->Reset(dpadHighlightImages[i], m_resLoader);
					break;//stop drawing process
				}

				//draw appropriate overlay based on button's state
				if (m_dPadDirectionTouches[i].size())
				{
					renderer.DrawRect(*m_dpadDirectionHighlightTextures[i], m_dPadDirectionHighlightRects[i]);
				}
			}
		}

		void InputDPad::DrawBoundingBoxesImpl(Video::IRectRenderer& renderer) {
			BaseInputPad::DrawBoundingBoxesImpl(renderer);

			Video::Color color = {.r = 1, .g = 0, .b=0, .a=1};
			renderer.DrawOutlineRect(color, m_outlineSize, m_dPadRect);
		}
	}
}