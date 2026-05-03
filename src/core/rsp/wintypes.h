#ifndef __WINTYPES_H__
#define __WINTYPES_H__
#if defined(__WIN32__) && !defined(PS3)
#include <sys/types.h>

typedef int HWND;
// Guard these definitions to prevent conflicts with winlnxdefs.h when compiling for PS3
//#if defined(__WIN32__) && !defined(PS3)
typedef int HINSTANCE;
typedef void* LPVOID;

#define __declspec(dllexport)
#define __cdecl
#define _cdecl
#define WINAPI

typedef unsigned int		DWORD;
typedef unsigned short		WORD;
typedef unsigned char			BYTE, byte;
typedef int			BOOL, BOOLEAN;
#define __int8                  char
#define __int16                 short
#define __int32	                int
#define __int64                 long long

/** HRESULT stuff **/
typedef long				HRESULT;
#define S_OK				((HRESULT)0L)
#define E_NOTIMPL		0x80004001L

#ifndef FALSE
# define FALSE (0)
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif

typedef int HMENU;
typedef int RECT;
typedef int PAINTSTRUCT;

#endif // defined(__WIN32__) && !defined(PS3)

#endif /* __WINTYPES_H__ */
