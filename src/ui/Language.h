// language.h
#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <string>
#include <map>

enum LanguageID {
    LANG_EN, // Inglés
    LANG_ES, // Español
    // Añade más idiomas según sea necesario
    LANG_COUNT
};

struct LanguageStrings {
    std::map<std::string, std::string> strings;

    std::string get(const std::string& key) const {
        std::map<std::string, std::string>::const_iterator it = strings.find(key); // Cambiado 'auto it' al tipo explícito
        if (it != strings.end()) {
            return it->second;
        }
        return key; // Devolver la clave si no se encuentra la traducción
    }
};

extern LanguageStrings currentLanguage;
extern LanguageID currentLanguageID;

bool loadLanguage(LanguageID lang);

#ifdef PS3
LanguageID getConsoleLanguage();
#endif

#endif // LANGUAGE_H
