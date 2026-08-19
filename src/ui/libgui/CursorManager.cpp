/**
 * Wii64 - CursorManager.cpp
 * Copyright (C) 2009 sepp256
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#include "CursorManager.h"
#include "InputManager.h"
#include "FocusManager.h"
#ifdef __GX__
#include "GraphicsGX.h"
#else //__GX__
#include "GraphicsRSX.h"
#endif //!__GX__
#include "Image.h"
#include "resources.h"
#include "../MenuResources.h"
#include "IPLFont.h"
#include "Frame.h"
#include "../../main/wii64config.h"
#ifdef PS3
#include <io/pad.h>
#define PS3_BTN_CROSS (1<<6)
extern u32 display_width;
extern u32 display_height;
#endif

namespace menu {

Cursor::Cursor()
		: currentFrame(0),
		  cursorList(0),
		  cursorX((float)(::display_width ? ::display_width : 1920) * 0.5f),
		  cursorY((float)(::display_height ? ::display_height : 1080) * 0.5f),
		  cursorRot(0.0f),
		  imageCenterX(12.0f),
		  imageCenterY(4.0f),
		  foundComponent(0),
		  hoverOverComponent(0),
		  pressed(false),
		  frameSwitch(true),
		  clearInput(true),
		  freezeAction(false),
		  buttonsPressed(0),
		  activeChan(-1)
{
	pointerImage = new menu::Image(CursorPointTexture, 40, 56, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	grabImage = new menu::Image(CursorGrabTexture, 40, 44, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
}

Cursor::~Cursor()
{
	delete pointerImage;
	delete grabImage;
}

void Cursor::updateCursor()
{
	if (hoverOverComponent) hoverOverComponent->setFocus(false);
	if(frameSwitch) 
	{
		setCursorFocus(NULL);
		clearInput = true;
		frameSwitch = false;
	}

#ifdef HW_RVL
	WPADData* wiiPad = Input::getInstance().getWpad();
	for (int i = 0; i < 4; i++)
	{
	//cycle through all 4 wiimotes
	//take first one pointing at screen
	//if (aimed at screen): set cursorX, cursorY, cursorRot, clear focusActive
		if(wiiPad[i].ir.valid && wiiPad[i].err == WPAD_ERR_NONE)
		{
			if(activeChan != i)
			{
				//clear previous cursor state here
				previousButtonsPressed[i] = wiiPad[i].btns_h;
				activeChan = i;
//				clearInput = false;
			}
			else
			{
				if(clearInput) 
				{
					previousButtonsPressed[i] = wiiPad[i].btns_h;
				}
				clearInput = false;
			}
			if(screenMode)	cursorX = wiiPad[i].ir.x*848/640 - 104;
			else			cursorX = wiiPad[i].ir.x;
			cursorY = wiiPad[i].ir.y; 
			cursorRot = wiiPad[i].ir.angle;
			buttonsPressed = (wiiPad[i].btns_h ^ previousButtonsPressed[i]) & wiiPad[i].btns_h;
			previousButtonsPressed[i] = wiiPad[i].btns_h;
			pressed = (wiiPad[i].btns_h & (WPAD_BUTTON_A | WPAD_BUTTON_B)) ? true : false;
			Focus::getInstance().setFocusActive(false);
			if (hoverOverComponent) 
			{
				hoverOverComponent->setFocus(false);
				hoverOverComponent = NULL;
			}
			std::vector<CursorEntry>::iterator iteration;
			for (iteration = cursorList.begin(); iteration != cursorList.end(); iteration++)
			{
				if(	currentFrame == (*iteration).frame &&
					(cursorX > (*iteration).xRange[0]) && (cursorX < (*iteration).xRange[1]) &&
					(cursorY > (*iteration).yRange[0]) && (cursorY < (*iteration).yRange[1]))
				{
					setCursorFocus((*iteration).comp);
					if (frameSwitch) break;
				}
			}
			if (!hoverOverComponent) setCursorFocus(currentFrame);
			return;
		}
	}
#endif

#ifdef PS3
	// PS3: analog stick cursor control (Left stick moves cursor, Cross = click)
	// Use pad data already read by Input::refreshInput() to avoid consuming
	// pad data a second time (ioPadGetData is one-shot on RPCS3).
	{
		const padData* paddata = Input::getInstance().getPS3PadData();
		if (paddata && paddata->len > 0)
		{
			// Left stick: button[4]=LX, button[5]=LY (0-255, center=128)
			s8 lx = (s8)(paddata->button[4] - 128);
			s8 ly = (s8)(paddata->button[5] - 128);

			// Dead zone
			if (lx < -15 || lx > 15) cursorX += lx * 0.5f;
			if (ly < -15 || ly > 15) cursorY -= ly * 0.5f; // Y inverted

			// Clamp to screen
			if (cursorX < 0) cursorX = 0;
			if (cursorX > ::display_width) cursorX = ::display_width;
			if (cursorY < 0) cursorY = 0;
			if (cursorY > ::display_height) cursorY = ::display_height;

			// Cross button = click (button[3] bit 6 = PS3_BTN_CROSS)
			u16 btns = ((paddata->button[2]&0xFF)<<8) | (paddata->button[3]&0xFF);
			pressed = (btns & PS3_BTN_CROSS) ? true : false;
			buttonsPressed = pressed ? 1 : 0;

			Focus::getInstance().setFocusActive(false);
			if (hoverOverComponent) 
			{
				hoverOverComponent->setFocus(false);
				hoverOverComponent = NULL;
			}
			// Map cursor screen coords to 640x480 logical space for hit-testing
			// (ChannelButton positions are in 640x480)
			float hitX = cursorX * 640.0f / ::display_width;
			float hitY = cursorY * 480.0f / ::display_height;
			std::vector<CursorEntry>::iterator iteration;
			for (iteration = cursorList.begin(); iteration != cursorList.end(); iteration++)
			{
				if(	currentFrame == (*iteration).frame &&
					(hitX > (*iteration).xRange[0]) && (hitX < (*iteration).xRange[1]) &&
					(hitY > (*iteration).yRange[0]) && (hitY < (*iteration).yRange[1]))
				{
					setCursorFocus((*iteration).comp);
					if (frameSwitch) break;
				}
			}
			if (!hoverOverComponent) setCursorFocus(currentFrame);
			return;
		}
	}
#endif

	//if not: clear cursorX, cursorY, cursorRot, set focusActive
	cursorX = cursorY = cursorRot = 0.0f;
	setCursorFocus(NULL);
	Focus::getInstance().setFocusActive(true);
	activeChan = -1;
}

void Cursor::setCursorFocus(Component* component)
{
	int buttonsDown = 0;
	int focusDirection = 0;
	Component* newHoverOverComponent = NULL;

#ifdef HW_RVL
	if (buttonsPressed & WPAD_BUTTON_A) buttonsDown |= Focus::ACTION_SELECT;
	if (buttonsPressed & WPAD_BUTTON_B) buttonsDown |= Focus::ACTION_BACK;
#endif
#ifdef PS3
	if (pressed) buttonsDown |= Focus::ACTION_SELECT;
#endif
	if (freezeAction) buttonsDown = 0;
	if (component) newHoverOverComponent = component->updateFocus(focusDirection,buttonsDown);
	if (newHoverOverComponent) 
	{
		if (hoverOverComponent) hoverOverComponent->setFocus(false);
		hoverOverComponent = newHoverOverComponent;
	}

}

void Cursor::drawCursor(Graphics& gfx)
{
	int width, height;
	{
		gfx.enableBlending(true);
		gfx.setTEV(GX_REPLACE);

		gfx.setDepth(-10.0f);
		gfx.newModelView();
		gfx.rotate(cursorRot);
		gfx.translate(-imageCenterX, -imageCenterY, 0.0f);
		gfx.translateApply(cursorX, cursorY, 0.0f);
		gfx.loadModelView();
		gfx.loadOrthographic();

		if(pressed)
		{
			grabImage->activateImage(GX_TEXMAP0);
			width = 40;
			height = 44;
		}
		else
		{
			pointerImage->activateImage(GX_TEXMAP0);
			width = 40;
			height = 56;
		}
		gfx.drawImage(0, 0, 0, width, height, 0.0, 1.0, 0.0, 1.0);
	}

/*	GXColor debugColor = (GXColor) {255, 100, 100, 255};
	IplFont::getInstance().drawInit(debugColor);
	char buffer[50];
	sprintf(buffer, "IR: %.2f, %.2f, %.2f",cursorX,cursorY,cursorRot);
	IplFont::getInstance().drawString((int) 320, (int) 240, buffer, 1.0, true);*/
}

void Cursor::addComponent(Frame* parentFrame, Component* component, float x1, float x2, float y1, float y2)
{
	CursorEntry entry;
	entry.frame = parentFrame;
	entry.comp = component;
	entry.xRange[0] = x1;
	entry.xRange[1] = x2;
	entry.yRange[0] = y1;
	entry.yRange[1] = y2;
	cursorList.push_back(entry);
}

void Cursor::removeComponent(Frame* parentFrame, Component* component)
{
	std::vector<CursorEntry>::iterator iter;

	for(iter = cursorList.begin(); iter != cursorList.end(); ++iter)
	{
		if((*iter).frame == parentFrame && (*iter).comp == component)
		{
			cursorList.erase(iter);
			break;
		}
	}
}

void Cursor::setCurrentFrame(Frame* frame)
{
	currentFrame = frame;
	frameSwitch = true;
}

Frame* Cursor::getCurrentFrame()
{
	return currentFrame;
}

void Cursor::clearInputData()
{
	clearInput = true;
}

void Cursor::clearCursorFocus()
{
	if (hoverOverComponent) hoverOverComponent->setFocus(false);
	cursorX = cursorY = 0.0f;
}

void Cursor::setFreezeAction(bool freeze)
{
	freezeAction = freeze;
}

} //namespace menu 
