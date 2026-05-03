// language.cpp
#include "Language.h"
#include <stdio.h>
#include <string>
#include <string.h>

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
    const char* filename;
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

    FILE* file = fopen(filename, "r");
    if (!file) {
        return false;
    }

    currentLanguage.strings.clear();
    currentLanguageID = lang;

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || strlen(line) < 3) continue;
        
        char* eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char* key = line;
            char* value = eq + 1;
            // Eliminar salto de línea al final
            value[strcspn(value, "\r\n")] = 0;
            currentLanguage.strings[std::string(key)] = std::string(value);
        }
    }
    fclose(file);
    return true;
}