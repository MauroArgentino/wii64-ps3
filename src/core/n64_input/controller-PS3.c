/**
 * ps364 - controller-PS3.c
 * Copyright (C) 2007, 2008, 2009, 2010 Mike Slegeir
 * Copyright (C) 2007, 2008, 2009, 2010, 2011 sepp256
 * 
 * PS3 controller input module
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
 *                sepp256@gmail.com
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


#include <string.h>
#include <io/pad.h>
#include "../../debug.h"
#include "controller.h"

enum {
	L_STICK_AS_ANALOG = 1, R_STICK_AS_ANALOG = 2,
};

enum {
	PS3_BTN_LEFT		= (1<<15),
	PS3_BTN_DOWN		= (1<<14),
	PS3_BTN_RIGHT		= (1<<13),
	PS3_BTN_UP			= (1<<12),
	PS3_BTN_START		= (1<<11),
	PS3_BTN_R3			= (1<<10),
	PS3_BTN_L3			= (1<<9),
	PS3_BTN_SELECT		= (1<<8),
	PS3_BTN_SQUARE		= (1<<7),
	PS3_BTN_CROSS		= (1<<6),
	PS3_BTN_CIRCLE		= (1<<5),
	PS3_BTN_TRIANGLE	= (1<<4),
	PS3_BTN_R1			= (1<<3),
	PS3_BTN_L1			= (1<<2),
	PS3_BTN_R2			= (1<<1),
	PS3_BTN_L2			= (1<<0),
	L_STICK_L			= 0x01 << 16,
	L_STICK_R			= 0x02 << 16,
	L_STICK_U			= 0x04 << 16,
	L_STICK_D			= 0x08 << 16,
	R_STICK_L			= 0x10 << 16,
	R_STICK_R			= 0x20 << 16,
	R_STICK_U			= 0x40 << 16,
	R_STICK_D			= 0x80 << 16,
};

static button_t buttons[] = {
	{  0, ~0,				"None" },
	{  1, PS3_BTN_UP,		"D-Up" },
	{  2, PS3_BTN_LEFT,		"D-Left" },
	{  3, PS3_BTN_RIGHT,	"D-Right" },
	{  4, PS3_BTN_DOWN,		"D-Down" },
	{  5, PS3_BTN_L1,		"L1" },
	{  6, PS3_BTN_L2,		"L2" },
	{  7, PS3_BTN_L3,		"L3" },
	{  8, PS3_BTN_R1,		"R1" },
	{  9, PS3_BTN_R2,		"R2" },
	{ 10, PS3_BTN_R3,		"R3" },
	{ 11, PS3_BTN_CROSS,	"Cross" },
	{ 12, PS3_BTN_SQUARE,	"Square" },
	{ 13, PS3_BTN_CIRCLE,	"Circle" },
	{ 14, PS3_BTN_TRIANGLE,	"Triangle" },
	{ 15, PS3_BTN_START,	"Start" },
	{ 16, PS3_BTN_SELECT,	"Select" },
	{ 17, R_STICK_U,		"RS-Up" },
	{ 18, R_STICK_L,		"RS-Left" },
	{ 19, R_STICK_R,		"RS-Right" },
	{ 20, R_STICK_D,		"RS-Down" },
	{ 21, L_STICK_U,		"LS-Up" },
	{ 22, L_STICK_L,		"LS-Left" },
	{ 23, L_STICK_R,		"LS-Right" },
	{ 24, L_STICK_D,		"LS-Down" },
};

static button_t analog_sources[] = {
	{ 0, L_STICK_AS_ANALOG,  "Left Stick" },
	{ 1, R_STICK_AS_ANALOG,  "Right Stick" },
};

static button_t menu_combos[] = {
	{ 0, PS3_BTN_SQUARE|PS3_BTN_TRIANGLE,	"Square+Triangle" },
	{ 1, PS3_BTN_START|PS3_BTN_SELECT,		"Start+Select" },
};

static u32 getButtons(u32 buttonsPS3, u32 analogPS3)
{
	//0xRH-RV-LH-LV 0x00 = Left/Up, 0xFF = Right/Down
	u32 b = buttonsPS3;
	s8 LstickX      = (s8) ((int)((analogPS3>>8) & 0xFF) - 128);
	int rawLY        = (int)((analogPS3>>0) & 0xFF) - 128;
	s8 LstickY       = rawLY == -128 ? (s8)127 : (s8)(-rawLY);
	s8 RstickX      = (s8) ((int)((analogPS3>>24) & 0xFF) - 128);
	int rawRY        = (int)((analogPS3>>16) & 0xFF) - 128;
	s8 RstickY       = rawRY == -128 ? (s8)127 : (s8)(-rawRY);
	
	int dz = 18;
	if(LstickX    < -dz) b |= L_STICK_L;
	if(LstickX    >  dz) b |= L_STICK_R;
	if(LstickY    < -dz) b |= L_STICK_U;
	if(LstickY    >  dz) b |= L_STICK_D;
	
	if(RstickX    < -dz) b |= R_STICK_L;
	if(RstickX    >  dz) b |= R_STICK_R;
	if(RstickY    < -dz) b |= R_STICK_U;
	if(RstickY    >  dz) b |= R_STICK_D;
	
	return b;
}

padInfo padinfo;

static u32 previousButtonsPS3[4];
static u32 previousAnalogPS3[4] = {0x80808080, 0x80808080, 0x80808080, 0x80808080};
static u32 edgeButtons[4];       /* buttons detected as rising-edge pressed */
static int edgeHoldCount[4];     /* frames to keep edge-detected buttons held */
static int refreshCounter;       /* throttle refreshAvailable() calls */

/* Shared pad cache: polled once per cycle from the interpreter loop.
 * Both _GetKeys and ps3_pad_exit_combo_pressed read from here.
 * This avoids the RPCS3 ioPadGetData consumption bug where the exit combo
 * steals pad data before _GetKeys can read it. */
static padData shared_pad[4];
static u32 shared_buttons[4];
static u32 shared_analog[4];

/* Forward declarations */
static void refreshAvailable(void);

/* OSD pad status: visible from VI.cpp for on-screen debug */
char osd_pad_status[128] = "pad: init";

/* Lazy controller reassignment: when a new pad is detected, trigger
 * auto_assign_controllers() so that Controls[].Present and virtualControllers
 * get set even if the controller wasn't available at ROM load time. */
static void poll_pad_reassign_if_needed(void)
{
	static u8 prev_avail[4] = {0,0,0,0};
	int i, need_reassign = 0;
	u8 cur_avail[4];

	refreshAvailable();

	for (i = 0; i < 4; i++) {
		cur_avail[i] = controller_PS3.available[i];
		if (cur_avail[i] && !prev_avail[i])
			need_reassign = 1;
	}

	if (need_reassign)
		auto_assign_controllers();

	for (i = 0; i < 4; i++)
		prev_avail[i] = cur_avail[i];
}

/* Poll all available pads once. Called from the interpreter loop.
 * Both _GetKeys and ps3_pad_exit_combo_pressed read from here.
 *
 * CRITICAL: Only update shared_buttons/shared_analog when len > 0.
 * On RPCS3, ioPadGetData() "consumes" pad data — the first call returns
 * len > 0 with valid button[], but subsequent calls return len = 0.
 * If we overwrite shared_buttons with the zeroed button[] from a len=0
 * read, the button press is lost before _GetKeys can consume it. */
void controller_PS3_poll_pad(void)
{
	int i;

	/* Periodically check for new controllers and reassign.
	 * Every 64 poll cycles (~0.5s at 0x1FFF poll rate). */
	static int reassign_divider = 0;
	reassign_divider++;
	if ((reassign_divider & 0x3F) == 0)
		poll_pad_reassign_if_needed();

	for (i = 0; i < 4; i++)
	{
		if (!controller_PS3.available[i])
		{
			shared_pad[i].len = 0;
			continue;
		}
		memset(&shared_pad[i], 0, sizeof(padData));
		ioPadGetData(i, &shared_pad[i]);
		/* Only overwrite when pad reports new data (len > 0).
		 * This preserves the last known button/analog state so
		 * _GetKeys always has valid data even between events. */
		if (shared_pad[i].len > 0) {
			shared_buttons[i] = ((shared_pad[i].button[2]&0xFF)<<8) | (shared_pad[i].button[3]&0xFF);
			shared_analog[i] = ((shared_pad[i].button[4]&0xFF)<<24) | ((shared_pad[i].button[5]&0xFF)<<16) |
			                   ((shared_pad[i].button[6]&0xFF)<<8)  | ((shared_pad[i].button[7]&0xFF)&0xFF);
		}
	}
}

#ifdef DEBUG_PROBES
static unsigned int input_probe_cnt;
#endif

static int _GetKeys(int Control, BUTTONS * Keys, controller_config_t* config)
{
	u32 buttonsPS3, analogPS3;
	BUTTONS* c = Keys;
	memset(c, 0, sizeof(BUTTONS));

	/* Throttled pad availability check: every 30 PIF reads instead of every
	 * one. Calling ioPadGetInfo() every frame can interfere with RPCS3's
	 * DualSense pad handler, causing analog drift and phantom inputs. */
	refreshCounter++;
	if (refreshCounter >= 30 || !controller_PS3.available[Control]) {
		refreshAvailable();
		refreshCounter = 0;
	}

	if (!controller_PS3.available[Control]) return 0;

	/* OSD diagnostic: shows pad state and why _GetKeys might fail */
	{
		static unsigned int osd_cnt = 0;
		osd_cnt++;
		if ((osd_cnt & 0x3) == 0) {
			padData *sp = &shared_pad[Control];
			snprintf(osd_pad_status, sizeof(osd_pad_status),
				"P%d: btn=%04X raw=%02X%02X%02X%02X%02X%02X",
				Control,
				shared_buttons[Control],
				sp->button[2], sp->button[3],
				sp->button[4], sp->button[5],
				sp->button[6], sp->button[7]);
		}
	}

#ifdef DEBUG_PROBES
	{
		static int gk_cnt = 0;
		static u32 gk_skip = 0;
		if (gk_cnt < 8 || (gk_cnt < 200 && (gk_skip++ % 500) == 0))
		{
			gk_cnt++;
			DBG_INP("[GETKEYS] Control=%d len=%u\n", Control, shared_pad[Control].len);
		}
	}
#endif

	buttonsPS3 = shared_buttons[Control];
	analogPS3 = shared_analog[Control];

	/* Edge detection: latch rising-edge presses only when pad reports
	 * a state change (len > 0). This prevents re-latching on every
	 * poll cycle. */
	if (shared_pad[Control].len > 0)
	{
		u32 newlyPressed = shared_buttons[Control] & ~edgeButtons[Control];
		edgeButtons[Control] = shared_buttons[Control];
		if (newlyPressed)
			edgeHoldCount[Control] = 8;
	}

	/* Merge edge-detected buttons into live state so even when paddata.len==0
	 * the N64 still sees the press for up to 8 reads after it happened. */
	if (edgeHoldCount[Control] > 0)
	{
		buttonsPS3 |= edgeButtons[Control];
		edgeHoldCount[Control]--;
	}

	u32 b = getButtons(buttonsPS3,analogPS3);
	inline int isHeld(button_tp button){
		return (b & button->mask) == button->mask;
	}
	
	c->R_DPAD       = isHeld(config->DR);
	c->L_DPAD       = isHeld(config->DL);
	c->D_DPAD       = isHeld(config->DD);
	c->U_DPAD       = isHeld(config->DU);
	
	c->START_BUTTON = isHeld(config->START);
	c->B_BUTTON     = isHeld(config->B);
	c->A_BUTTON     = isHeld(config->A);

	c->Z_TRIG       = isHeld(config->Z);
	c->R_TRIG       = isHeld(config->R);
	c->L_TRIG       = isHeld(config->L);

	c->R_CBUTTON    = isHeld(config->CR);
	c->L_CBUTTON    = isHeld(config->CL);
	c->D_CBUTTON    = isHeld(config->CD);
	c->U_CBUTTON    = isHeld(config->CU);

	if(config->analog->mask == L_STICK_AS_ANALOG){
		c->X_AXIS = (s8)  ((int)((analogPS3>>8) & 0xFF) - 128);
		int rawY = (int)((analogPS3>>0) & 0xFF) - 128;
		c->Y_AXIS = rawY == -128 ? (s8)127 : (s8)(-rawY);
	} else if(config->analog->mask == R_STICK_AS_ANALOG){
		c->X_AXIS = (s8)  ((int)((analogPS3>>24) & 0xFF) - 128);
		int rawY = (int)((analogPS3>>16) & 0xFF) - 128;
		c->Y_AXIS = rawY == -128 ? (s8)127 : (s8)(-rawY);
	}
	if(config->invertedY) c->Y_AXIS = -c->Y_AXIS;

	// Return whether the exit button(s) are pressed
	if (isHeld(config->exit))
	{
		previousButtonsPS3[Control] &= ~config->exit->mask;
		return 1;
	}
	else
		return 0;
}

static void pause(int Control){
}

static void resume(int Control){ }

static void rumble(int Control, int rumble){
}

static void configure(int Control, controller_config_t* config){
	// Don't know how this should be integrated
}

static void assign(int p, int v){
	// Nothing to do here
}


controller_t controller_PS3 =
	{ 'P',
	  _GetKeys,
	  configure,
	  assign,
	  pause,
	  resume,
	  rumble,
	  refreshAvailable,
	  {0, 0, 0, 0},
	  sizeof(buttons)/sizeof(buttons[0]),
	  buttons,
	  sizeof(analog_sources)/sizeof(analog_sources[0]),
	  analog_sources,
	  sizeof(menu_combos)/sizeof(menu_combos[0]),
	  menu_combos,
	  { .DU        = &buttons[1],  // D-Pad Up
	    .DL        = &buttons[2],  // D-Pad Left
	    .DR        = &buttons[3],  // D-Pad Right
	    .DD        = &buttons[4],  // D-Pad Down
	    .Z         = &buttons[6],  // L2
	    .L         = &buttons[5],  // L1
	    .R         = &buttons[8],  // R1
	    .A         = &buttons[11], // Cross
	    .B         = &buttons[13], // Circle
	    .START     = &buttons[15], // Start
	    .CU        = &buttons[17], // Right Stick Up
	    .CL        = &buttons[18], // Right Stick Left
	    .CR        = &buttons[19], // Right Stick Right
	    .CD        = &buttons[20], // Right Stick Down
	    .analog    = &analog_sources[0],
	    .exit      = &menu_combos[0],
	    .invertedY = 0,
	  }
	 };

static void refreshAvailable(void){
	//Note: 7 controllers can be connected to PS3. Maybe check up to 7 in the future?
	ioPadGetInfo(&padinfo);
	
	int i;
	for(i=0; i<4; ++i) {
		u8 was_available = controller_PS3.available[i];
		controller_PS3.available[i] = padinfo.status[i];
		/* Enable PRESS_ON exactly once per controller connection.
		 * Calling ioPadSetPortSetting repeatedly floods RPCS3 with
		 * DualSense send_output_report errors → eventual crash. */
		if (padinfo.status[i] && !was_available)
			ioPadSetPortSetting(i, 0x02);
	}
#ifdef DEBUG_PROBES
	DBG_INP("[PADINFO] status=%d%d%d%d avail=%d%d%d%d\n",
		padinfo.status[0], padinfo.status[1], padinfo.status[2], padinfo.status[3],
		controller_PS3.available[0], controller_PS3.available[1],
		controller_PS3.available[2], controller_PS3.available[3]);
#endif
}

/* Poll del combo de salida directamente del pad (ioPadGetData), sin depender
 * de que la ROM lea el PIF. Permite volver al menu aunque el juego no procese
 * los controles (p.ej. DK64 atascado en el boot). Devuelve 1 si el combo
 * configurado de salida (por defecto Square+Triangle) esta presionado. */
int ps3_pad_exit_combo_pressed(void)
{
	int i;
	button_tp combo = &menu_combos[0];

	if (virtualControllers[0].config && virtualControllers[0].config->exit)
		combo = virtualControllers[0].config->exit;

	for (i = 0; i < 4; i++)
	{
		if (!controller_PS3.available[i]) continue;
		if ((shared_buttons[i] & combo->mask) == combo->mask)
			return 1;
	}
	return 0;
}
