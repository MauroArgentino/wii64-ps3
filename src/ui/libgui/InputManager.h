/**
 * Wii64 - InputManager.h
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

#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "GuiTypes.h"
#ifdef PS3
#include <ppu-types.h>
#include <io/pad.h>
#define PS3_MAX_PADS 7
#endif

namespace menu {

class Input
{
public:
	void refreshInput();
#ifdef HW_RVL
	WPADData* getWpad();
#endif
#ifdef __GX__
	PADStatus* getPad();
#endif //__GX__
	void clearInputData();
#ifdef PS3
	u16* getPS3Buttons() { return ps3Buttons; }
	const padData* getPS3PadData() const { return ps3PadData; }
#endif
	static Input& getInstance()
	{
		static Input obj;
		return obj;
	}

private:
	Input();
	~Input();
#ifdef __GX__
	PADStatus gcPad[4];
#endif //__GX__
#ifdef HW_RVL
	WPADData *wiiPad;
#endif
#ifdef PS3
	u16 ps3Buttons[PS3_MAX_PADS];
	padData ps3PadData[PS3_MAX_PADS];
#endif

};

} //namespace menu 

#endif
