/*
 * Windows regedit.exe registry editor implementation.
 *
 * Copyright 2002 Andriy Palamarchuk
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "regedit.h"
#include <strsafe.h>

BOOL import_registry_file(FILE *reg_file);
BOOL export_registry_key(const TCHAR *file_name, TCHAR *path, BOOL unicode);
void delete_registry_key(TCHAR *reg_key_name);

static WCHAR const

STRING_CANNOT_OPEN_FILE
[]= L"regedit: Unable to open the file '%s'.\n",

STRING_UNHANDLED_ACTION
[]= L"regedit: Unhandled action.\n",

STRING_INVALID_SWITCH
[]= L"regedit: Invalid or unrecognized switch [%s]\n",

STRING_HELP
[]= L"Type \"regedt33 /?\" for help.\n",

STRING_NO_FILENAME
[]= L"regedit: No filename was specified.\n",

STRING_NO_REG_KEY
[]= L"regedit: No registry key was specified for removal.\n",

STRING_PRESS_ENTER_TO_CLOSE
[]= L"\nPress ENTER to close",

STRING_USAGE
[]= L"Usage:\n"
    L"  %s [options] [filename] [reg_key]\n"
    L"\n"
    L"Options:\n"
    L"  [no option]    Launch the graphical version of this program.\n"
    L"  /D             Delete a specified registry key.\n"
    L"  /E             Export the contents of a specified registry key to a file.\n"
    L"                 If no key is specified, the entire registry is exported.\n"
    L"  /U             When used with [/E], changes file encoding to Unicode.\n"
    L"  /?             Display this information and exit.\n"
    L"  [filename]     The location of the file containing registry information to\n"
    L"                 be imported. When used with [/E], this option specifies the\n"
    L"                 file location where registry information will be exported.\n"
    L"  [reg_key]      The registry key to be modified.\n";

#ifndef _WIN32_WCE
static void output_writeconsole(const WCHAR *str)
{
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == NULL)
    {
        AllocConsole();
        hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (DWORD const len = WideCharToMultiByte(CP_OEMCP, 0, str, -1, NULL, 0, NULL, NULL))
    {
        char *const msg = static_cast<char *>(_alloca(len));
        WideCharToMultiByte(CP_OEMCP, 0, str, -1, msg, len, NULL, NULL);
        WriteFile(hStdOut, msg, len, NULL, FALSE);
    }
}
#endif

static void output_message(const WCHAR *fmt, ...)
{
    va_list va_args;

    va_start(va_args, fmt);
#ifndef _WIN32_WCE
    WCHAR str[1024];
    StringCchVPrintfW(str, _countof(str), fmt, va_args);
    output_writeconsole(str);
#else
    vwprintf(fmt, va_args);
#endif
    va_end(va_args);
}

static void error_exit(const WCHAR *fmt, ...)
{
    va_list va_args;

    va_start(va_args, fmt);
#ifndef _WIN32_WCE
    WCHAR str[1024];
    StringCchVPrintfW(str, _countof(str), fmt, va_args);
    output_writeconsole(str);
#else
    vwprintf(fmt, va_args);
#endif
    va_end(va_args);

#ifndef _WIN32_WCE
    // If we wrote stuff to the console, keep the process up until the user hits the enter key.
    // Reuses code from https://github.com/monero-project/monero/blob/master/src/common/util.cpp.
    // Copyright (c) 2014-2023, The Monero Project
    // SPDX-License-Identifier: BSD-3-Clause
    HANDLE hConIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hConIn != INVALID_HANDLE_VALUE)
    {
        output_writeconsole(STRING_PRESS_ENTER_TO_CLOSE);

        DWORD oldMode;
        GetConsoleMode(hConIn, &oldMode);
        SetConsoleMode(hConIn, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);

        FlushConsoleInputBuffer(hConIn);

        DWORD read;
        ReadConsoleW(hConIn, str, _countof(str), &read, NULL);

        SetConsoleMode(hConIn, oldMode);
        CloseHandle(hConIn);
    }
#endif

    exit(0); /* regedit.exe always terminates with error code zero */
}

enum REGEDIT_ACTION { ACTION_ADD, ACTION_EXPORT, ACTION_DELETE };

static void PerformRegAction(REGEDIT_ACTION action, BOOL unicode, WCHAR **argv, int *i)
{
    switch (action) {
    case ACTION_ADD: {
            WCHAR *filename = argv[*i];
            FILE *reg_file = _wfopen(filename, L"rb");
            if (reg_file == NULL)
                error_exit(STRING_CANNOT_OPEN_FILE, filename);
            import_registry_file(reg_file);
            fclose(reg_file);
            break;
        }
    case ACTION_DELETE: {
            delete_registry_key(argv[*i]);
            break;
        }
    case ACTION_EXPORT: {
            WCHAR *filename = argv[*i];
            WCHAR *key_name = argv[++(*i)];
            if (key_name && *key_name)
                export_registry_key(filename, key_name, unicode);
            else
                export_registry_key(filename, NULL, unicode);
            break;
        }
    default: {
            error_exit(STRING_UNHANDLED_ACTION);
            break;
        }
    }
}

BOOL ProcessCmdLine(int argc, wchar_t *argv[])
{
    if (argc == 1)
        return FALSE;

    REGEDIT_ACTION action = ACTION_ADD;
    BOOL unicode = FALSE;

    int i = 1;
    for (; i < argc; i++)
    {
        if (argv[i][0] != '/')
            break; /* No flags specified. */

        switch (towupper(argv[i][1]))
        {
        case '?':
            error_exit(STRING_USAGE, FindLastComponent(argv[0]));
            break;
        case 'D':
            action = ACTION_DELETE;
            break;
        case 'E':
            action = ACTION_EXPORT;
            break;
        case 'U':
            unicode = TRUE;
            break;
        default:
            output_message(STRING_INVALID_SWITCH, argv[i]);
            error_exit(STRING_HELP);
        }
    }

    if (i == argc)
    {
        switch (action)
        {
        case ACTION_ADD:
        case ACTION_EXPORT:
            output_message(STRING_NO_FILENAME);
            break;
        case ACTION_DELETE:
            output_message(STRING_NO_REG_KEY);
            break;
        }
        error_exit(STRING_HELP);
    }

    for (; i < argc; i++)
        PerformRegAction(action, unicode, argv, &i);

    return TRUE;
}
