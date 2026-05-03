#ifndef WIIMENU_H
#define WIIMENU_H

#include <string>
#include <vector>
#include "Language.h"
#include "libgui/Button.h" // Asumiendo que libgui es genérico
#include "libgui/Frame.h"  // Asumiendo que libgui es genérico
#include "MenuAudioSynthesizer.h" // Renombrado

// Estructura para representar un "Canal" al estilo Wii
struct MenuChannel {
    std::string id;
    std::string titleKey; // Clave para el sistema de Language.h
    uint32_t color;       // Color de fondo si no hay textura
    int iconId;           // ID del recurso de imagen
    bool visible;
};

// Clase para cada "Canal" como un componente independiente
class ChannelButton : public menu::Button {
public:
    ChannelButton(const MenuChannel& data, float x, float y, float w, float h);
    virtual void drawComponent(menu::Graphics& gfx);
    void updateAnimation(float pulse, float transitionProgress);
private:
    MenuChannel channelData;
    float displayScale;   // Escala visual suave
    float targetScale;    // Escala a la que queremos llegar
    float baseW, baseH;
    float transitionProgress; // 0.0 a 1.0
};

class MenuManager : public menu::Frame {
public:
    MenuManager();
    ~MenuManager();

    void init();
    void update(uint32_t padInput); // Maneja la navegación con el DualShock
    void drawChildren(menu::Graphics& gfx); // Reemplaza al renderizador manual

private:
    std::vector<MenuChannel> channels;
    std::vector<ChannelButton*> buttonComponents;
    int columns;
    int rows;

    // Parámetros de animación (estilo Wii)
    float selectionScale;
    uint32_t lastInput;
    
    // Parámetros de transición
    bool isTransitioning;
    float transitionProgress;
    int transitionedButtonIndex;

    void executeAction(const std::string& id);
};

#endif // WIIMENU_H