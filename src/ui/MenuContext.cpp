/**
 * Wii64 - MenuContext.cpp
 * Copyright (C) 2009, 2010 sepp256
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

#include "MenuContext.h"
#include "libgui/FocusManager.h"
#include "libgui/CursorManager.h"
#include "libgui/MessageBox.h"
#include "MenuManager.h"

MenuContext *pMenuContext;
extern "C" char menuActive;
extern "C" uint16_t PAD_ButtonsHeld(int);
extern "C" void resumeAudio();

static MenuManager* g_mainMenu = NULL; // Renombrado
static bool g_mainMenuInited = false; // Renombrado

MenuContext::MenuContext(GXRModeObj *vmode)
		: currentActiveFrame(0),
		  mainFrame(0),
		  loadRomFrame(0),
		  fileBrowserFrame(0),
		  currentRomFrame(0),
		  loadSaveFrame(0),
		  saveGameFrame(0),
		  settingsFrame(0),
		  selectCPUFrame(0),
		  configureInputFrame(0),
		  configurePaksFrame(0),
		  configureButtonsFrame(0)
{
	pMenuContext = this;
//	dbg_printf("Initialize MenuContext\r\n");

	menu::Gui::getInstance().setVmode(vmode);
//	dbg_printf("MenuContext - setVmode done\r\n");

	mainFrame = new MainFrame();
	g_mainMenu = new MenuManager(); // Renombrado
//	dbg_printf("MenuContext - new MainFrame\r\n");
	loadRomFrame = new LoadRomFrame();
	fileBrowserFrame = new FileBrowserFrame();
	currentRomFrame = new CurrentRomFrame();
	loadSaveFrame = new LoadSaveFrame();
	saveGameFrame = new SaveGameFrame();
	settingsFrame = new SettingsFrame();
	selectCPUFrame = new SelectCPUFrame();
	configureInputFrame = new ConfigureInputFrame();
	configurePaksFrame = new ConfigurePaksFrame();
	configureButtonsFrame = new ConfigureButtonsFrame();

	menu::Gui::getInstance().addFrame(mainFrame);
	menu::Gui::getInstance().addFrame(g_mainMenu); // Renombrado
//	dbg_printf("MenuContext - add MainFrame\r\n");
	menu::Gui::getInstance().addFrame(loadRomFrame);
	menu::Gui::getInstance().addFrame(fileBrowserFrame);
	menu::Gui::getInstance().addFrame(currentRomFrame);
	menu::Gui::getInstance().addFrame(loadSaveFrame);
	menu::Gui::getInstance().addFrame(saveGameFrame);
	menu::Gui::getInstance().addFrame(settingsFrame);
	menu::Gui::getInstance().addFrame(selectCPUFrame);
	menu::Gui::getInstance().addFrame(configureInputFrame);
	menu::Gui::getInstance().addFrame(configurePaksFrame);
	menu::Gui::getInstance().addFrame(configureButtonsFrame);

	menu::Focus::getInstance().setFocusActive(true);
//	dbg_printf("MenuContext - setFocusActive\r\n");
	setActiveFrame(FRAME_MAIN);
//	dbg_printf("Initialized MenuContext\r\n");
}

MenuContext::~MenuContext()
{
	delete configureButtonsFrame;
	delete configurePaksFrame;
	delete configureInputFrame;
	delete selectCPUFrame;
	delete settingsFrame;
	delete saveGameFrame;
	delete loadSaveFrame;
	delete currentRomFrame;
	delete fileBrowserFrame;
	delete loadRomFrame;
	delete mainFrame;
	pMenuContext = NULL;
}

bool MenuContext::isRunning()
{
	bool isRunning = true;
//	dbg_printf("MenuContext isRunning\r\n");
//	printf("MenuContext isRunning\n");

	bool msgVisible = menu::MessageBox::getInstance().getActive();

    // Si el emulador ha marcado que el menú ya no está activo, detenemos el renderizado del GUI
    if (menuActive == 0) {
        g_mainMenu->setVisible(false);
        mainFrame->setVisible(false);
        return false; // Salimos del bucle del menú para dejar paso al juego
    }

    // Refresh pad data ONCE at the top, so all consumers (MenuManager,
    // Cursor, Focus) read from the same cached snapshot instead of
    // each calling ioPadGetData and consuming the data for the next reader.
    menu::Input::getInstance().refreshInput();

    // Si estamos en el menú principal y no hay mensajes, el foco debe ser del WiiMenu
    if (currentActiveFrame == mainFrame && !msgVisible) { // 'mainFrame' es el frame base, no el menú de canales
        if (!g_mainMenuInited) { // Renombrado
            g_mainMenu->init(); // Renombrado
            g_mainMenuInited = true; // Renombrado
        }
        if (menu::Focus::getInstance().getCurrentFrame() != g_mainMenu) { // Renombrado
            menu::Focus::getInstance().clearPrimaryFocus();
            menu::Focus::getInstance().setCurrentFrame(g_mainMenu); // Renombrado
            menu::Cursor::getInstance().setCurrentFrame(g_mainMenu); // Sincronizar cursor con el menú de canales
            g_mainMenu->reset(); // <--- IMPORTANTE: Limpia el estado al recuperar el foco
            // Ya no forzamos setDefaultFocus aquí; g_mainMenu lo gestiona internamente
            // en su función update() para recordar el último botón tocado.
            menu::Focus::getInstance().clearInputData(); // Limpia estados de botones previos
        }
        mainFrame->setVisible(false); // Oculta el mainFrame genérico
        g_mainMenu->setVisible(true); // Muestra el menú de canales
        g_mainMenu->update(0); // Actualiza el menú de canales (padInput ya no se usa directamente)
    } else {
        if (g_mainMenu) g_mainMenu->setVisible(false); // Oculta el menú de canales si no es el activo
        if (currentActiveFrame == mainFrame) mainFrame->setVisible(true);
    }

    // Si el menú se desactivó (ej. al pulsar Jugar), salimos sin dibujar el frame final
    if (menuActive == 0) return false;

	draw();

/*	PADStatus* gcPad = menu::Input::getInstance().getPad();
	if(gcPad[0].button & PAD_BUTTON_START)
		isRunning = false;*/
	
	return isRunning;
}

void MenuContext::setActiveFrame(int frameIndex)
{
	if(currentActiveFrame)
		currentActiveFrame->hideFrame();

	// Al navegar a un frame que no es el principal, ocultamos el menú de canales
	// (tarjetas) y reseteamos su estado de transición para evitar que queden dibujadas
	// detrás de File Browser / Settings (Gui::draw dibuja todos los frames cada frame).
	if (frameIndex != FRAME_MAIN && g_mainMenu) {
		g_mainMenu->setVisible(false);
		g_mainMenu->resetTransition();
	}

	switch(frameIndex) {
	case FRAME_MAIN:
		currentActiveFrame = mainFrame;
		break;
	case FRAME_LOADROM:
		currentActiveFrame = loadRomFrame;
		break;
	case FRAME_FILEBROWSER:
		currentActiveFrame = fileBrowserFrame;
		break;
	case FRAME_CURRENTROM:
		currentActiveFrame = currentRomFrame;
		break;
	case FRAME_LOADSAVE:
		currentActiveFrame = loadSaveFrame;
		break;
	case FRAME_SAVEGAME:
		currentActiveFrame = saveGameFrame;
		break;
	case FRAME_SETTINGS:
		currentActiveFrame = settingsFrame;
		break;
	case FRAME_SELECTCPU:
		currentActiveFrame = selectCPUFrame;
		break;
	case FRAME_CONFIGUREINPUT:
		currentActiveFrame = configureInputFrame;
		break;
	case FRAME_CONFIGUREPAKS:
		currentActiveFrame = configurePaksFrame;
		break;
	case FRAME_CONFIGUREBUTTONS:
		currentActiveFrame = configureButtonsFrame;
		break;
	}

	if(currentActiveFrame)
	{
		currentActiveFrame->showFrame();
		menu::Focus::getInstance().setCurrentFrame(currentActiveFrame);
		menu::Cursor::getInstance().setCurrentFrame(currentActiveFrame);
	}
}

void MenuContext::setActiveFrame(int frameIndex, int submenu)
{
	setActiveFrame(frameIndex);
	if(currentActiveFrame) currentActiveFrame->activateSubmenu(submenu);
}

void MenuContext::showChannelMenu(bool show)
{
	if (!g_mainMenu) return;
	if (show) {
		// Volver al menú de canales: mostrar tarjetas, ocultar el frame genérico
		g_mainMenu->resetTransition();
		g_mainMenu->setVisible(true);
		if (mainFrame) mainFrame->setVisible(false);
	} else {
		// Mostrar un modal (MessageBox) sobre el fondo. El bg.tx lo dibuja
		// Gui::drawBackground() siempre, así que ocultamos AMBOS menús (el de
		// canales y el frame genérico con sus widgets) para que solo se vea el fondo.
		g_mainMenu->resetTransition();
		g_mainMenu->setVisible(false);
		if (mainFrame) mainFrame->setVisible(false);
	}
}

menu::Frame* MenuContext::getFrame(int frameIndex)
{
	menu::Frame* pFrame = NULL;
	switch(frameIndex) {
	case FRAME_MAIN:
		pFrame = mainFrame;
		break;
	case FRAME_LOADROM:
		pFrame = loadRomFrame;
		break;
	case FRAME_FILEBROWSER:
		pFrame = fileBrowserFrame;
		break;
	case FRAME_CURRENTROM:
		pFrame = currentRomFrame;
		break;
	case FRAME_LOADSAVE:
		pFrame = loadSaveFrame;
		break;
	case FRAME_SAVEGAME:
		pFrame = saveGameFrame;
		break;
	case FRAME_SETTINGS:
		pFrame = settingsFrame;
		break;
	case FRAME_SELECTCPU:
		pFrame = selectCPUFrame;
		break;
	case FRAME_CONFIGUREINPUT:
		pFrame = configureInputFrame;
		break;
	case FRAME_CONFIGUREBUTTONS:
		pFrame = configureButtonsFrame;
		break;
	}

	return pFrame;
}

void MenuContext::draw()
{
	menu::Gui::getInstance().draw();
}
