// language.cpp
#include "Language.h"
#include <fstream>
#include <sstream>

#ifdef PS3
// #include <sys/sysconf.h>
// #include <stdio.h>
#endif

LanguageStrings currentLanguage;
LanguageID currentLanguageID = LANG_ES; // Idioma por defecto

#ifdef PS3
LanguageID getConsoleLanguage() {
    return LANG_ES; // Forzamos el idioma inglés
}
#else
LanguageID getConsoleLanguage() {
    return LANG_ES; // Por defecto para otras plataformas
}
#endif

bool loadLanguage(LanguageID lang) {
    std::string filename;
    switch (lang) {
        case LANG_EN:
            filename = "resources/languages/lang_en.txt";
            break;
        case LANG_ES:
            filename = "resources/languages/lang_es.txt";
            break;
        // Añade más casos para otros idiomas (aunque ahora solo se cargará inglés)
        default:
            currentLanguageID = LANG_EN;
            filename = "resources/languages/lang_en.txt";
            break;
    }

    // Usa filename.c_str() para obtener un const char*
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        return false;
    }

    currentLanguage.strings.clear();
    currentLanguageID = lang;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Ignorar líneas vacías o comentarios
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            currentLanguage.strings[key] = value;
        }
    }
    file.close();
    return true;
}