/**
 * Mupen64 - winlnxdefs.h
 * Copyright (C) 2002 Hacktarux
 *
 * Mupen64 homepage: http://mupen64.emulation64.com
 * email address: hacktarux@yahoo.fr
 * 
 * If you want to contribute to the project please contact
 * me first (maybe someone is already making what you are
 * planning to do).
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
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#ifndef WINLNXDEFS_H
#define WINLNXDEFS_H

#include <ppu-types.h>

#ifndef BOOL
typedef u32 BOOL;
#endif
#ifndef DWORD
typedef u32 DWORD;
#endif
#ifndef WORD
typedef unsigned short WORD;
#endif
#ifndef BYTE
typedef unsigned char BYTE;
#endif
#ifndef UINT
typedef u32 UINT;
#endif
typedef unsigned long long DWORD64;

typedef short SHORT;

typedef s32 __int32;

typedef int HINSTANCE;
typedef int HWND;
typedef int WPARAM;
typedef int LPARAM;
typedef void* LPVOID;

#define __declspec(dllexport)
#define _cdecl
#define __stdcall
#define WINAPI

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifdef __cplusplus
// Fix para el namespace __gnu_debug que falta en algunos headers de Estwald PSDK3v2
namespace std { namespace __gnu_debug { } }
namespace __gnu_debug { }

// Solución para errores internos de libstdc++ en Estwald PSDK3v2
// Se usan macros variádicas para capturar cualquier número de argumentos y silenciarlos.

#undef __glibcxx_requires_nonempty
#define __glibcxx_requires_nonempty(...) ((void)0)
#undef __glibcxx_requires_valid_range
#define __glibcxx_requires_valid_range(...) ((void)0)
#undef __glibcxx_requires_sorted
#define __glibcxx_requires_sorted(...) ((void)0)

// Fixes adicionales para errores relacionados con aserciones de depuración de C++
#undef _GLIBCXX_DEBUG_ASSERT
#define _GLIBCXX_DEBUG_ASSERT(...) ((void)0)
#undef _GLIBCXX_DEBUG_PEDASSERT
#define _GLIBCXX_DEBUG_PEDASSERT(...) ((void)0)
#undef __glibcxx_requires_string
#define __glibcxx_requires_string(...) ((void)0)
#endif

#endif // WINLNXDEFS_H
