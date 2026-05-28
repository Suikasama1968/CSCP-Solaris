/*
 * Solaris compatibility layer for Common Source Code Project.
 * Keep this included with:
 *   -include source/src/solaris/osd_compat.h
 * Copyright (c) 2026 M.Yoshiyama
 */

#ifndef CSP_SOLARIS_COMPAT_H
#define CSP_SOLARIS_COMPAT_H


#define __EXTENSIONS__ 1
#define _REENTRANT 1
#define _POSIX_PTHREAD_SEMANTICS 1

#if defined(__sun) && !defined(__SOLARIS__)
#define __SOLARIS__ 1
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#ifdef __cplusplus
#include <string>
#include <algorithm>
#include <cctype>
#endif
#include <wchar.h>

#ifndef _MAX_PATH
#define _MAX_PATH 2048
#endif

#ifndef MAX_PATH
#define MAX_PATH 2048
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_TEXT
#define O_TEXT 0
#endif

#ifndef LONG_PTR
typedef intptr_t LONG_PTR;
#endif
#ifndef INT_PTR
typedef intptr_t INT_PTR;
#endif
#ifndef UINT_PTR
typedef uintptr_t UINT_PTR;
#endif
#ifndef DWORD_PTR
typedef uintptr_t DWORD_PTR;
#endif

#ifndef SUPPORT_TCHAR_TYPE
#ifndef _TCHAR
typedef char _TCHAR;
#endif
#ifndef __T
#define __T(x) x
#endif
#ifndef _T
#define _T(x) __T(x)
#endif
#ifndef _TEXT
#define _TEXT(x) __T(x)
#endif
#endif

#ifndef stricmp
#define stricmp(a,b) strcasecmp(a,b)
#endif
#ifndef strnicmp
#define strnicmp(a,b,n) strncasecmp(a,b,n)
#endif
#ifndef _stricmp
#define _stricmp(a,b) strcasecmp(a,b)
#endif
#ifndef _strnicmp
#define _strnicmp(a,b,n) strncasecmp(a,b,n)
#endif
#ifndef _ftprintf
#define _ftprintf fprintf
#endif

// Win32 API compatibility helpers.
#ifndef ZeroMemory
#define ZeroMemory(p,s) memset((p), 0, (s))
#endif
#ifndef CopyMemory
#define CopyMemory(d,s,n) memcpy((d), (s), (n))
#endif

static inline void solaris_sleep_ms(unsigned int ms)
{
    usleep((useconds_t)ms * 1000);
}

#ifndef Sleep
#define Sleep(ms) solaris_sleep_ms((ms))
#endif

// Some CSP code expects Win32 virtual key codes.
#ifndef VK_BACK
#define VK_BACK        0x08
#define VK_TAB         0x09
#define VK_RETURN      0x0d
#define VK_SHIFT       0x10
#define VK_CONTROL     0x11
#define VK_MENU        0x12
#define VK_PAUSE       0x13
#define VK_CAPITAL     0x14
#define VK_ESCAPE      0x1b
#define VK_SPACE       0x20
#define VK_PRIOR       0x21
#define VK_NEXT        0x22
#define VK_END         0x23
#define VK_HOME        0x24
#define VK_LEFT        0x25
#define VK_UP          0x26
#define VK_RIGHT       0x27
#define VK_DOWN        0x28
#define VK_INSERT      0x2d
#define VK_DELETE      0x2e
#define VK_0           0x30
#define VK_1           0x31
#define VK_2           0x32
#define VK_3           0x33
#define VK_4           0x34
#define VK_5           0x35
#define VK_6           0x36
#define VK_7           0x37
#define VK_8           0x38
#define VK_9           0x39
#define VK_A           0x41
#define VK_B           0x42
#define VK_C           0x43
#define VK_D           0x44
#define VK_E           0x45
#define VK_F           0x46
#define VK_G           0x47
#define VK_H           0x48
#define VK_I           0x49
#define VK_J           0x4a
#define VK_K           0x4b
#define VK_L           0x4c
#define VK_M           0x4d
#define VK_N           0x4e
#define VK_O           0x4f
#define VK_P           0x50
#define VK_Q           0x51
#define VK_R           0x52
#define VK_S           0x53
#define VK_T           0x54
#define VK_U           0x55
#define VK_V           0x56
#define VK_W           0x57
#define VK_X           0x58
#define VK_Y           0x59
#define VK_Z           0x5a
#define VK_NUMPAD0     0x60
#define VK_NUMPAD1     0x61
#define VK_NUMPAD2     0x62
#define VK_NUMPAD3     0x63
#define VK_NUMPAD4     0x64
#define VK_NUMPAD5     0x65
#define VK_NUMPAD6     0x66
#define VK_NUMPAD7     0x67
#define VK_NUMPAD8     0x68
#define VK_NUMPAD9     0x69
#define VK_MULTIPLY    0x6a
#define VK_ADD         0x6b
#define VK_SEPARATOR   0x6c
#define VK_SUBTRACT    0x6d
#define VK_DECIMAL     0x6e
#define VK_DIVIDE      0x6f
#define VK_F1          0x70
#define VK_F2          0x71
#define VK_F3          0x72
#define VK_F4          0x73
#define VK_F5          0x74
#define VK_F6          0x75
#define VK_F7          0x76
#define VK_F8          0x77
#define VK_F9          0x78
#define VK_F10         0x79
#define VK_F11         0x7a
#define VK_F12         0x7b
#define VK_OEM_1       0xba
#define VK_OEM_PLUS    0xbb
#define VK_OEM_COMMA   0xbc
#define VK_OEM_MINUS   0xbd
#define VK_OEM_PERIOD  0xbe
#define VK_OEM_2       0xbf
#define VK_OEM_3       0xc0
#define VK_OEM_4       0xdb
#define VK_OEM_5       0xdc
#define VK_OEM_6       0xdd
#define VK_OEM_7       0xde
#define VK_OEM_102     0xe2
#endif

#endif /* CSP_SOLARIS_COMPAT_H */
