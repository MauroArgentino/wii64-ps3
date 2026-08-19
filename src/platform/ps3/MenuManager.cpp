#include "../../ui/MenuManager.h" // Renombrado
#include <ppu-types.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../../video/glN64/OpenGL.h" // Imaginando la nueva ruta limpia
#include "../../ui/libgui/GraphicsRSX.h" // Incluir GraphicsRSX directamente para PS3
#include "../../ui/Language.h"           // Necesario para menu::Language
#include "../../ui/libgui/FocusManager.h" // Se añade para la definición completa de menu::Focus
#include "../../ui/libgui/InputManager.h" // For Input::getPS3PadData()
#include "../../ui/libgui/IPLFont.h"      // Necesario para menu::IPLFont
#include "../../ui/libgui/GuiResources.h"
#include "../../ui/libgui/Image.h"        // Asumiendo que Image es genérico
#include "../../ui/MenuElementStyle.h"    // Renombrado
#include "../../ui/libgui/CursorManager.h" // Para Cursor::addComponent()
#include "../../debug.h"

#ifndef GX_TEXMAP0
#define GX_TEXMAP0 0
#endif

extern gcmContextData *context;
extern "C" char menuActive;

// Funciones externas de MainFrame.cpp para las acciones
void Func_LoadROM();
void Func_Settings();
void Func_Credits();
void Func_ExitToLoader();
void Func_PlayGame();

MenuManager::MenuManager() : columns(4), rows(3), lastInput(0) {
    selectionScale = 1.0f;
    isTransitioning = false;
    transitionProgress = 0.0f;
    transitionedButtonIndex = -1;
}

MenuManager::~MenuManager() {}

void MenuManager::init() {
    MenuChannel ch;

    // Inicializar canales clásicos
    ch.id = "LOAD_ROM";
    ch.titleKey = "MENU_LOAD_ROM";
    ch.color = 0xFFFFFFFF;
    ch.iconId = menu::Resources::IMAGE_STYLEA_BUTTON;
    ch.visible = true;
    channels.push_back(ch);

    ch.id = "SETTINGS";
    ch.titleKey = "MENU_SETTINGS";
    ch.color = 0xF2F2F2FF;
    ch.iconId = menu::Resources::IMAGE_STYLEA_BUTTONFOCUS;
    ch.visible = true;
    channels.push_back(ch);

    ch.id = "SAVES";
    ch.titleKey = "MENU_SAVES";
    ch.color = 0xF2F2F2FF;
    ch.iconId = menu::Resources::IMAGE_STYLEA_BUTTONSELECTOFF;
    ch.visible = true;
    channels.push_back(ch);

    ch.id = "RESUME";
    ch.titleKey = "MENU_RESUME";
    ch.color = 0xDDDDDDFF;
    ch.iconId = menu::Resources::IMAGE_N64_CONTROLLER;
    ch.visible = true;
    channels.push_back(ch);

    ch.id = "CREDITS";
    ch.titleKey = "MENU_CREDITS";
    ch.color = 0xFFFFFFFF;
    ch.iconId = menu::Resources::IMAGE_N64_CONTROLLER;
    ch.visible = true;
    channels.push_back(ch);

    ch.id = "EXIT";
    ch.titleKey = "MENU_EXIT";
    ch.color = 0xFFCCCCFF;
    ch.iconId = menu::Resources::IMAGE_STYLEA_BUTTONSELECTON;
    ch.visible = true;
    channels.push_back(ch);

    // Rellenar vacíos para mantener la estética de la cuadrícula
    while(channels.size() < 12) {
        ch.id = "EMPTY"; ch.titleKey = ""; ch.color = 0xEEEEEEFF; ch.iconId = -1; ch.visible = false;
        channels.push_back(ch);
    }

    g_menuAudioSynthesizer.init(); // Renombrado

    // Crear los componentes de botón
    float margin = 16.0f;
    float chW = 140.0f; float chH = 100.0f;
    float startX = 32.0f; float startY = 80.0f;

    for (int i = 0; i < (int)channels.size(); ++i) {
        float x = startX + (i % columns) * (chW + margin);
        float y = startY + (i / columns) * (chH + margin);
        ChannelButton* btn = new ChannelButton(channels[i], x, y, chW, chH); // Renombrado
        
        // Asignar acciones globales
        if      (channels[i].id == "LOAD_ROM") btn->setClicked(Func_LoadROM);
        else if (channels[i].id == "SETTINGS") btn->setClicked(Func_Settings);
        else if (channels[i].id == "CREDITS")  btn->setClicked(Func_Credits);
        else if (channels[i].id == "RESUME")   btn->setClicked(Func_PlayGame);
        else if (channels[i].id == "EXIT")     btn->setClicked(Func_ExitToLoader);

        add(btn);
        buttonComponents.push_back(btn);

        // Registrar con el cursor para hit-test Wii-style
        if (channels[i].visible) {
            menu::Cursor::getInstance().addComponent(this, btn, x, x + chW, y, y + chH);
        }
    }

    // Configurar navegación entre botones (Focus neighbors)
    for (int i = 0; i < (int)buttonComponents.size(); ++i) {
        int row = i / columns; // 'columns' es miembro de MenuManager
        int col = i % columns;
        if (col < columns - 1 && i + 1 < (int)buttonComponents.size())
            buttonComponents[i]->setNextFocus(menu::Focus::DIRECTION_RIGHT, buttonComponents[i+1]);
        if (col > 0)
            buttonComponents[i]->setNextFocus(menu::Focus::DIRECTION_LEFT, buttonComponents[i-1]);
        if (row < rows - 1 && i + columns < (int)buttonComponents.size())
            buttonComponents[i]->setNextFocus(menu::Focus::DIRECTION_DOWN, buttonComponents[i+columns]);
        if (row > 0)
            buttonComponents[i]->setNextFocus(menu::Focus::DIRECTION_UP, buttonComponents[i-columns]);
    }
    setDefaultFocus(buttonComponents[0]);
}

void MenuManager::reset() {
    // Limpiar estados de botones para evitar bloqueos al volver de una ROM
    isTransitioning = false;
    transitionProgress = 0.0f;
    transitionedButtonIndex = -1;
    lastInput = 0; // Forzamos un reset del estado del pad
    if (!buttonComponents.empty()) {
        setDefaultFocus(buttonComponents[0]);
    }
}

static float pulse = 0.0f;
static float g_tvStaticTime = 0.0f;
void MenuManager::update(uint32_t /*padInput*/) { // padInput ya no se usa directamente
    if (isTransitioning) {
        transitionProgress += 0.04f; // Velocidad de la transición
        if (transitionProgress >= 1.0f) {
            isTransitioning = false;
            executeAction(channels[transitionedButtonIndex].id);
            transitionProgress = 0.0f;
        }
        
        // Actualizar solo el botón que está animando
        buttonComponents[transitionedButtonIndex]->updateAnimation(pulse, transitionProgress);
        return;
    }

    pulse += 0.05f; // Más lento para mayor fluidez
    if (pulse > 6.28f) pulse = 0.0f; // Resetear ciclo de 2*PI
    g_tvStaticTime += 0.033f;

    // Leer entrada del pad usando datos cacheados por Input::refreshInput()
    uint32_t currentInput = 0;
    const padData* paddata = menu::Input::getInstance().getPS3PadData();
    u16* ps3Buttons = menu::Input::getInstance().getPS3Buttons();
    for(int i=0; i<7; i++){
        if(paddata && ps3Buttons[i]){
            currentInput |= ps3Buttons[i];
        }
    }
    uint32_t buttonsDown = (currentInput ^ lastInput) & currentInput;
    lastInput = currentInput;

    // NOTE: D-pad navigation is handled by FocusManager::updateFocus() in Gui::draw().
    // Do NOT duplicate moveFocus() calls here or focus moves twice per press.

    // Ejecutar acción si se presiona EQUIS
    if (buttonsDown & PS3_BTN_CROSS) {
        for (size_t i = 0; i < buttonComponents.size(); ++i) {
            if (buttonComponents[i]->getFocus()) {
                isTransitioning = true;
                transitionedButtonIndex = (int)i;
                transitionProgress = 0.0f;
                // Bloquear otros botones si fuera necesario
                break;
            }
        }
    }

    for(std::vector<ChannelButton*>::iterator it = buttonComponents.begin(); it != buttonComponents.end(); ++it) { // Renombrado
        (*it)->updateAnimation(pulse, 0.0f);
    }
}

void MenuManager::drawChildren(menu::Graphics& gfx) {
    // Asegurar que no hay recortes de pantalla de componentes previos
    gfx.disableScissor();

    // Si estamos en transición, dibujamos los demás botones con transparencia
    if (isTransitioning) {
        gfx.pushTransparency(1.0f - transitionProgress);
        for (int i = 0; i < (int)buttonComponents.size(); ++i) {
            if (i != transitionedButtonIndex) buttonComponents[i]->drawComponent(gfx);
        }
        gfx.popTransparency();
        // Dibujar el botón que hace el flip al final para que esté encima de todo
        buttonComponents[transitionedButtonIndex]->drawComponent(gfx);
    } else {
        menu::Frame::drawChildren(gfx);

        // Mostrar descripción del canal seleccionado en la parte inferior
        for (int i = 0; i < (int)buttonComponents.size(); ++i) {
            if (buttonComponents[i]->getFocus() && channels[i].visible && channels[i].id != "EMPTY") {
                std::string langStr = currentLanguage.get(channels[i].titleKey);
                const char* description = NULL;

                // Si el sistema devuelve la misma clave, significa que no encontró la traducción
                if (langStr != channels[i].titleKey && !langStr.empty()) {
                    description = langStr.c_str();
                } else {
                    // Fallback manual en español
                    if      (channels[i].id == "LOAD_ROM") description = "Cargar ROM";
                    else if (channels[i].id == "SETTINGS") description = "Opciones";
                    else if (channels[i].id == "SAVES")    description = "Gestionar Saves";
                    else if (channels[i].id == "RESUME")   description = "Reanudar Juego"; // Corregido el typo
                    else if (channels[i].id == "CREDITS")  description = "Créditos";
                    else if (channels[i].id == "EXIT")     description = "Salir";
                }

                if (description && description[0] != '\0') {
                    // Usamos coordenadas 640x480 que es lo que el motor GUI espera
                    // Inicializar la fuente con el color de la sombra
                    menu::IplFont::getInstance().drawInit((GXColor){0,0,0,255});
                    // Sombra (Negra)
                    menu::IplFont::getInstance().drawString(322, 442, (char*)description, 0.85f, true);
                    // Inicializar la fuente con el color del texto principal
                    menu::IplFont::getInstance().drawInit((GXColor){255,255,255,255});
                    // Texto (Blanco)
                    menu::IplFont::getInstance().drawString(320, 440, (char*)description, 0.85f, true);
                }
                break;
            }
        }
    }

    // --- Validación Visual de Mando (Debug Overlay) ---
    char debugBuf[256];
    std::string activeButtons = "";
    if (lastInput & PS3_BTN_UP)       activeButtons += "UP ";
    if (lastInput & PS3_BTN_DOWN)     activeButtons += "DOWN ";
    if (lastInput & PS3_BTN_LEFT)     activeButtons += "LEFT ";
    if (lastInput & PS3_BTN_RIGHT)    activeButtons += "RIGHT ";
    if (lastInput & PS3_BTN_CROSS)    activeButtons += "X ";
    if (lastInput & PS3_BTN_CIRCLE)   activeButtons += "O ";
    if (lastInput & PS3_BTN_SQUARE)   activeButtons += "[] ";
    if (lastInput & PS3_BTN_TRIANGLE) activeButtons += "/\\ ";
    if (lastInput & PS3_BTN_START)    activeButtons += "START ";
    if (lastInput & PS3_BTN_SELECT)   activeButtons += "SELECT ";
    if (lastInput & PS3_BTN_L1)       activeButtons += "L1 ";
    if (lastInput & PS3_BTN_R1)       activeButtons += "R1 ";
    if (lastInput & PS3_BTN_L2)       activeButtons += "L2 ";
    if (lastInput & PS3_BTN_R2)       activeButtons += "R2 ";

    snprintf(debugBuf, sizeof(debugBuf), "PAD: %04X [%s]", lastInput, activeButtons.c_str());

    // Dibujar información de depuración en la esquina superior izquierda
    menu::IplFont::getInstance().drawInit((GXColor){0, 0, 0, 255});
    menu::IplFont::getInstance().drawString(22, 22, debugBuf, 0.45f, false);
    menu::IplFont::getInstance().drawInit((GXColor){0, 255, 255, 255}); // Cyan
    menu::IplFont::getInstance().drawString(20, 20, debugBuf, 0.45f, false);
}

ChannelButton::ChannelButton(const MenuChannel& data, float x, float y, float w, float h) // Renombrado
    : menu::Button(0, NULL, x, y, w, h), channelData(data), displayScale(1.0f), targetScale(1.0f), baseW(w), baseH(h), transitionProgress(0.0f) {
    setActive(true);
    setVisible(true);
}

void ChannelButton::updateAnimation(float pulse, float transitionProg) {
    transitionProgress = transitionProg;
    if (transitionProgress > 0.0f) return; // Prioridad a la transición

    if (getFocus()) {
        // Efecto Wii: 10% más grande + un pequeño latido (sinf)
        targetScale = 1.12f + (sinf(pulse) * 0.015f);
    } else {
        targetScale = 1.0f;
    }
    // Lerp ajustado a 0.1f para una transición más sedosa (menos tosca)
    displayScale += (targetScale - displayScale) * 0.10f;
}

void ChannelButton::drawComponent(menu::Graphics& gfx) { // Renombrado
    // Centro original del botón
    float startCX = x + baseW / 2.0f;
    float startCY = y + baseH / 2.0f;
    float currentW = baseW * (transitionProgress > 0.0f ? 1.0f : displayScale);
    float currentH = baseH * (transitionProgress > 0.0f ? 1.0f : displayScale);

    float finalCX = startCX;
    float finalCY = startCY;
    float finalW = currentW;
    float finalH = currentH;

    // Si el botón está en transición, interpolamos hacia pantalla completa
    if (transitionProgress > 0.0f) {
        // El centro se mueve al centro de la pantalla (320, 240)
        finalCX = startCX + (320.0f - startCX) * transitionProgress;
        finalCY = startCY + (240.0f - startCY) * transitionProgress;
        // El tamaño crece a 640x480
        finalW = currentW + (640.0f - currentW) * transitionProgress;
        finalH = currentH + (480.0f - currentH) * transitionProgress;

        // Aplicar Efecto Flip Horizontal usando Coseno
        // Usamos una curva de coseno para el ancho (giro de 180 grados)
        float flipFactor = cosf(transitionProgress * 3.141592f);
        finalW = finalW * fabsf(flipFactor);
        
        // Simulación de perspectiva: el botón se encoge un poco en altura cuando está de lado
        float heightPersp = 1.0f - (0.15f * (1.0f - fabsf(flipFactor)));
        finalH *= heightPersp;
    }

    // Si el flip está en la segunda mitad (90-180 grados), invertimos visualmente el brillo
    uint32_t drawColor = channelData.color;
    if (transitionProgress > 0.5f) {
        // Oscurecer un poco el color para simular el "dorso" o la sombra del giro
        drawColor = ((drawColor & 0xFEFEFE00) >> 1) | (drawColor & 0xFF);
    }

    // Dibujamos el fondo del canal (Profundidad base)
    gfx.pushDepth(0.5f);
    if (channelData.id == "EMPTY") {
        drawChannelStatic(gfx, finalCX, finalCY, finalW, finalH, g_tvStaticTime);
    } else {
        drawChannelBackground(gfx, finalCX, finalCY, finalW, finalH, drawColor); // Renombrado
    }
    gfx.popDepth();

    // Dibujamos el marco de selección solo si no estamos en transición avanzada
    if (getFocus() && transitionProgress < 0.5f) {
        gfx.pushDepth(0.4f);
        drawSelectionFrame(gfx, finalCX, finalCY, finalW, finalH, drawColor);
        gfx.popDepth();
    }

    // Dibujar Ícono centrado y con blending
    if (channelData.iconId >= 0 && transitionProgress < 0.45f) {
        menu::Image* icon = menu::Resources::getInstance().getImage(channelData.iconId);
        if (icon) {
            icon->activateImage(GX_TEXMAP0);
            gfx.setTEV(GX_MODULATE);
            gfx.setColor((GXColor){255, 255, 255, 255});
            float iconSize = 72.0f * (transitionProgress > 0.0f ? 1.0f : displayScale) * fabsf(cosf(transitionProgress * 3.14159f));
            gfx.drawImage(0, (int)(finalCX - iconSize/2.0f), (int)(finalCY - iconSize/2.0f), 
                          (int)iconSize, (int)iconSize, 0, 1, 0, 1);
        }
    }
}

void MenuManager::executeAction(const std::string& id) { // Renombrado
    if (id == "EMPTY") return;
    
    DBG_UDP("Ejecutando acción: %s\n", id.c_str());
    
    if (id == "LOAD_ROM") Func_LoadROM();
    else if (id == "SETTINGS") Func_Settings();
    else if (id == "CREDITS") Func_Credits();
    else if (id == "RESUME") Func_PlayGame();
    else if (id == "EXIT") Func_ExitToLoader();
}