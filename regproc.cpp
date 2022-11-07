/*
 * Registry processing routines. Routines, common for registry
 * processing frontends.
 *
 * Copyright 1999 Sylvain St-Germain
 * Copyright 2002 Andriy Palamarchuk
 * Copyright 2008 Alexander N. Sørnes <alex@thehandofagony.com>
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

#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#define REG_VAL_BUF_SIZE        4096

static HKEY reg_class_keys[] = {
    HKEY_CLASSES_ROOT, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_USERS,
#ifndef _WIN32_WCE
    HKEY_CURRENT_CONFIG, HKEY_DYN_DATA,
#endif
};

static const TCHAR *reg_class_names[] = {
    _T("HKEY_CLASSES_ROOT"), _T("HKEY_CURRENT_USER"), _T("HKEY_LOCAL_MACHINE"), _T("HKEY_USERS"), 
#ifndef _WIN32_WCE
    _T("HKEY_CURRENT_CONFIG"), _T("HKEY_DYN_DATA"),
#endif
};

/******************************************************************************
 * Allocates memory and converts input from multibyte to wide chars
 * Returned string must be freed by the caller
 */
static WCHAR* GetWideString(const char* strA)
{
    if(strA)
    {
        WCHAR* strW;
        int len = MultiByteToWideChar(CP_ACP, 0, strA, -1, NULL, 0);

		strW = (WCHAR*)malloc(len * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, strA, -1, strW, len);
        return strW;
    }
    return NULL;
}

/******************************************************************************
 * Allocates memory and converts input from multibyte to wide chars
 * Returned string must be freed by the caller
 */
static WCHAR* GetWideStringN(const char* strA, int chars, DWORD *len)
{
    if(strA)
    {
        WCHAR* strW;
        *len = MultiByteToWideChar(CP_ACP, 0, strA, chars, NULL, 0);

        strW = (WCHAR*)malloc(*len * sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, strA, chars, strW, *len);
        return strW;
    }
    *len = 0;
    return NULL;
}

/******************************************************************************
 * Allocates memory and converts input from wide chars to multibyte
 * Returned string must be freed by the caller
 */
char* GetMultiByteString(const WCHAR* strW)
{
    if(strW)
    {
        char* strA;
        int len = WideCharToMultiByte(CP_ACP, 0, strW, -1, NULL, 0, NULL, NULL);

        strA = (char*)malloc(len);
        WideCharToMultiByte(CP_ACP, 0, strW, -1, strA, len, NULL, NULL);
        return strA;
    }
    return NULL;
}

/******************************************************************************
 * Allocates memory and converts input from wide chars to multibyte
 * Returned string must be freed by the caller
 */
static char* GetMultiByteStringN(const WCHAR* strW, int chars, DWORD* len)
{
    if(strW)
    {
        char* strA;
        *len = WideCharToMultiByte(CP_ACP, 0, strW, chars, NULL, 0, NULL, NULL);

		strA = (char*)malloc(*len);
        WideCharToMultiByte(CP_ACP, 0, strW, chars, strA, *len, NULL, NULL);
        return strA;
    }
    *len = 0;
    return NULL;
}

static TCHAR *(*get_line)(FILE *);

/* parser definitions */
enum parser_state
{
    HEADER,              /* parsing the registry file version header */
    PARSE_WIN31_LINE,    /* parsing a Windows 3.1 registry line */
    LINE_START,          /* at the beginning of a registry line */
    KEY_NAME,            /* parsing a key name */
    DELETE_KEY,          /* deleting a registry key */
    DEFAULT_VALUE_NAME,  /* parsing a default value name */
    QUOTED_VALUE_NAME,   /* parsing a double-quoted value name */
    DATA_START,          /* preparing for data parsing operations */
    DELETE_VALUE,        /* deleting a registry value */
    DATA_TYPE,           /* parsing the registry data type */
    STRING_DATA,         /* parsing REG_SZ data */
    DWORD_DATA,          /* parsing DWORD data */
    HEX_DATA,            /* parsing REG_BINARY, REG_NONE, REG_EXPAND_SZ or REG_MULTI_SZ data */
    EOL_BACKSLASH,       /* preparing to parse multiple lines of hex data */
    HEX_MULTILINE,       /* parsing multiple lines of hex data */
    UNKNOWN_DATA,        /* parsing an unhandled or invalid data type */
    SET_VALUE,           /* adding a value to the registry */
    NB_PARSER_STATES
};

struct parser
{
    FILE              *file;           /* pointer to a registry file */
    TCHAR              two_wchars[2];  /* first two characters from the encoding check */
    BOOL               is_unicode;     /* parsing Unicode or ASCII data */
    short int          reg_version;    /* registry file version */
    HKEY               hkey;           /* current registry key */
    TCHAR             *key_name;       /* current key name */
    TCHAR             *value_name;     /* value name */
    DWORD              parse_type;     /* generic data type for parsing */
    DWORD              data_type;      /* data type */
    void              *data;           /* value data */
    DWORD              data_size;      /* size of the data (in bytes) */
    BOOL               backslash;      /* TRUE if the current line contains a backslash */
    enum parser_state  state;          /* current parser state */
};

typedef TCHAR *(*parser_state_func)(struct parser *parser, TCHAR *pos);

/* parser state machine functions */
static TCHAR *header_state(struct parser *parser, TCHAR *pos);
static TCHAR *parse_win31_line_state(struct parser *parser, TCHAR *pos);
static TCHAR *line_start_state(struct parser *parser, TCHAR *pos);
static TCHAR *key_name_state(struct parser *parser, TCHAR *pos);
static TCHAR *delete_key_state(struct parser *parser, TCHAR *pos);
static TCHAR *default_value_name_state(struct parser *parser, TCHAR *pos);
static TCHAR *quoted_value_name_state(struct parser *parser, TCHAR *pos);
static TCHAR *data_start_state(struct parser *parser, TCHAR *pos);
static TCHAR *delete_value_state(struct parser *parser, TCHAR *pos);
static TCHAR *data_type_state(struct parser *parser, TCHAR *pos);
static TCHAR *string_data_state(struct parser *parser, TCHAR *pos);
static TCHAR *dword_data_state(struct parser *parser, TCHAR *pos);
static TCHAR *hex_data_state(struct parser *parser, TCHAR *pos);
static TCHAR *eol_backslash_state(struct parser *parser, TCHAR *pos);
static TCHAR *hex_multiline_state(struct parser *parser, TCHAR *pos);
static TCHAR *unknown_data_state(struct parser *parser, TCHAR *pos);
static TCHAR *set_value_state(struct parser *parser, TCHAR *pos);

static const parser_state_func parser_funcs[NB_PARSER_STATES] =
{
    header_state,              /* HEADER */
    parse_win31_line_state,    /* PARSE_WIN31_LINE */
    line_start_state,          /* LINE_START */
    key_name_state,            /* KEY_NAME */
    delete_key_state,          /* DELETE_KEY */
    default_value_name_state,  /* DEFAULT_VALUE_NAME */
    quoted_value_name_state,   /* QUOTED_VALUE_NAME */
    data_start_state,          /* DATA_START */
    delete_value_state,        /* DELETE_VALUE */
    data_type_state,           /* DATA_TYPE */
    string_data_state,         /* STRING_DATA */
    dword_data_state,          /* DWORD_DATA */
    hex_data_state,            /* HEX_DATA */
    eol_backslash_state,       /* EOL_BACKSLASH */
    hex_multiline_state,       /* HEX_MULTILINE */
    unknown_data_state,        /* UNKNOWN_DATA */
    set_value_state,           /* SET_VALUE */
};

/* set the new parser state and return the previous one */
static inline enum parser_state set_state(struct parser *parser, enum parser_state state)
{
    enum parser_state ret = parser->state;
    parser->state = state;
    return ret;
}

/******************************************************************************
 * Converts a hex representation of a DWORD into a DWORD.
 */
static BOOL convert_hex_to_dword(TCHAR *str, DWORD *dw)
{
    TCHAR *p, *end;
    int count = 0;

    while (*str == ' ' || *str == '\t') str++;
    if (!*str) goto error;

    p = str;
    while (_istxdigit(*p))
    {
        count++;
        p++;
    }
    if (count > 8) goto error;

    end = p;
    while (*p == ' ' || *p == '\t') p++;
    if (*p && *p != ';') goto error;

    *end = 0;
    *dw = _tcstoul(str, &end, 16);
    return TRUE;

error:
    return FALSE;
}

/******************************************************************************
 * Converts comma-separated hex data into a binary string and modifies
 * the input parameter to skip the concatenating backslash, if found.
 *
 * Returns TRUE or FALSE to indicate whether parsing was successful.
 */
static BOOL convert_hex_csv_to_hex(struct parser *parser, TCHAR **str)
{
    size_t size;
    BYTE *d;
    TCHAR *s;

    parser->backslash = FALSE;

    /* The worst case is 1 digit + 1 comma per byte */
    size = ((_tcslen(*str) + 1) / 2) + parser->data_size;
    parser->data = realloc(parser->data, size);

    s = *str;
    d = (BYTE *)parser->data + parser->data_size;

    while (*s)
    {
        TCHAR *end;
        unsigned long wc;

        wc = _tcstoul(s, &end, 16);
        if (wc > 0xff) return FALSE;

        if (s == end && wc == 0)
        {
            while (*end == ' ' || *end == '\t') end++;
            if (*end == '\\')
            {
                parser->backslash = TRUE;
                *str = end + 1;
                return TRUE;
            }
            else if (*end == ';')
                return TRUE;
            return FALSE;
        }

        *d++ = (BYTE)wc;
        parser->data_size++;

        if (*end && *end != ',')
        {
            while (*end == ' ' || *end == '\t') end++;
            if (*end && *end != ';') return FALSE;
            return TRUE;
        }

        if (*end) end++;
        s = end;
    }

    return TRUE;
}

/******************************************************************************
 * Parses the data type of the registry value being imported and modifies
 * the input parameter to skip the string representation of the data type.
 *
 * Returns TRUE or FALSE to indicate whether a data type was found.
 */
static BOOL parse_data_type(struct parser *parser, TCHAR **line)
{
    struct data_type { const TCHAR *tag; int len; int type; int parse_type; };

    static const struct data_type data_types[] = {
    /*    tag          len  type         parse type    */
        { _T("\""),     1,  REG_SZ,      REG_SZ     },
        { _T("hex:"),   4,  REG_BINARY,  REG_BINARY },
        { _T("dword:"), 6,  REG_DWORD,   REG_DWORD  },
        { _T("hex("),   4,  -1,          REG_BINARY }, /* REG_NONE, REG_EXPAND_SZ, REG_MULTI_SZ */
        { NULL,         0,  0,           0          }
    };

    const struct data_type *ptr;

    for (ptr = data_types; ptr->tag; ptr++)
    {
        if (_tcsncmp(ptr->tag, *line, ptr->len))
            continue;

        parser->parse_type = ptr->parse_type;
        parser->data_type = ptr->parse_type;
        *line += ptr->len;

        if (ptr->type == -1)
        {
            TCHAR *end;
            DWORD val;

            if (!**line || _totlower((*line)[1]) == 'x')
                return FALSE;

            /* "hex(xx):" is special */
            val = _tcstoul(*line, &end, 16);
            if (*end != ')' || *(end + 1) != ':' || (val == ~0u))
                return FALSE;

            parser->data_type = val;
            *line = end + 2;
        }
        return TRUE;
    }
    return FALSE;
}

/******************************************************************************
 * Replaces escape sequences with their character equivalents and
 * null-terminates the string on the first non-escaped double quote.
 *
 * Assigns a pointer to the remaining unparsed data in the line.
 * Returns TRUE or FALSE to indicate whether a closing double quote was found.
 */
static BOOL REGPROC_unescape_string(TCHAR *str, TCHAR **unparsed)
{
    int str_idx = 0;            /* current character under analysis */
    int val_idx = 0;            /* the last character of the unescaped string */
    int len = _tcslen(str);
    BOOL ret;

    for (str_idx = 0; str_idx < len; str_idx++, val_idx++) {
        if (str[str_idx] == '\\') {
            str_idx++;
            switch (str[str_idx]) {
            case 'n':
                str[val_idx] = '\n';
                break;
            case 'r':
                str[val_idx] = '\r';
                break;
            case '0':
                return FALSE;
            case '\\':
            case '"':
                str[val_idx] = str[str_idx];
                break;
            default:
                if (!str[str_idx]) return FALSE;
                //output_message(STRING_ESCAPE_SEQUENCE, str[str_idx]);
                str[val_idx] = str[str_idx];
                break;
            }
        } else if (str[str_idx] == '"') {
            break;
        } else {
            str[val_idx] = str[str_idx];
        }
    }

    ret = (str[str_idx] == '"');
    *unparsed = str + str_idx + 1;
    str[val_idx] = '\0';
    return ret;
}

static HKEY parse_key_name(TCHAR *key_name, TCHAR **key_path)
{
    unsigned int i;

    if (!key_name) return 0;

    *key_path = _tcschr(key_name, '\\');
    if (*key_path) (*key_path)++;

    for (i = 0; i < _countof(reg_class_keys); i++)
    {
        int len = _tcslen(reg_class_names[i]);
        if (!_tcsnicmp(key_name, reg_class_names[i], len) &&
           (key_name[len] == 0 || key_name[len] == '\\'))
        {
            return reg_class_keys[i];
        }
    }

    return 0;
}

static void close_key(struct parser *parser)
{
    if (parser->hkey)
    {
        free(parser->key_name);
        parser->key_name = NULL;

        RegCloseKey(parser->hkey);
        parser->hkey = NULL;
    }
}

/******************************************************************************
 * Opens the registry key given by the input path.
 * This key must be closed by calling close_key().
 */
static LONG open_key(struct parser *parser, TCHAR *path)
{
    HKEY key_class;
    TCHAR *key_path;
    LONG res;

    close_key(parser);

    /* Get the registry class */
    if (!path || !(key_class = parse_key_name(path, &key_path)))
        return ERROR_INVALID_PARAMETER;

    res = RegCreateKeyEx(key_class, key_path, 0, NULL, REG_OPTION_NON_VOLATILE,
                         KEY_ALL_ACCESS, NULL, &parser->hkey, NULL);

    if (res == ERROR_SUCCESS)
    {
        parser->key_name = (TCHAR *)malloc((_tcslen(path) + 1) * sizeof(TCHAR));
        _tcscpy(parser->key_name, path);
    }
    else
        parser->hkey = NULL;

    return res;
}

static void free_parser_data(struct parser *parser)
{
    if (parser->parse_type == REG_DWORD || parser->parse_type == REG_BINARY)
        free(parser->data);

    parser->data = NULL;
    parser->data_size = 0;
}

static void prepare_hex_string_data(struct parser *parser)
{
    if (parser->data_type == REG_EXPAND_SZ || parser->data_type == REG_MULTI_SZ ||
        parser->data_type == REG_SZ)
    {
        if (parser->is_unicode)
        {
			TCHAR *data = (TCHAR *)parser->data;
            DWORD len = parser->data_size / sizeof(TCHAR);

            if (data[len - 1] != 0)
            {
                data[len] = 0;
                parser->data_size += sizeof(TCHAR);
            }
        }
        else
        {
			BYTE *data = (BYTE *)parser->data;

            if (data[parser->data_size - 1] != 0)
            {
                data[parser->data_size] = 0;
                parser->data_size++;
            }
#ifdef _UNICODE
            parser->data = GetWideStringN((char*)parser->data, parser->data_size, &parser->data_size);
            parser->data_size *= sizeof(TCHAR);
            free(data);
#endif
        }
    }
}

enum reg_versions {
    REG_VERSION_31,
    REG_VERSION_40,
    REG_VERSION_50,
    REG_VERSION_FUZZY,
    REG_VERSION_INVALID
};

static enum reg_versions parse_file_header(const TCHAR *s)
{
    static const TCHAR header_31[] = _T("REGEDIT");

    while (*s == ' ' || *s == '\t') s++;

    if (!_tcscmp(s, header_31))
        return REG_VERSION_31;

    if (!_tcscmp(s, _T("REGEDIT4")))
        return REG_VERSION_40;

    if (!_tcscmp(s, _T("Windows Registry Editor Version 5.00")))
        return REG_VERSION_50;

    /* The Windows version accepts registry file headers beginning with "REGEDIT" and ending
     * with other characters, as long as "REGEDIT" appears at the start of the line. For example,
     * "REGEDIT 4", "REGEDIT9" and "REGEDIT4FOO" are all treated as valid file headers.
     * In all such cases, however, the contents of the registry file are not imported.
     */
    if (!_tcsncmp(s, header_31, 7)) /* "REGEDIT" without NUL */
        return REG_VERSION_FUZZY;

    return REG_VERSION_INVALID;
}

/* handler for parser HEADER state */
static TCHAR *header_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line, *header;

    if (!(line = get_line(parser->file)))
        return NULL;

    if (!parser->is_unicode)
    {
        header = (TCHAR *)malloc((_tcslen(line) + 3) * sizeof(TCHAR));
        header[0] = parser->two_wchars[0];
        header[1] = parser->two_wchars[1];
        _tcscpy(header + 2, line);
        parser->reg_version = parse_file_header(header);
        free(header);
    }
    else parser->reg_version = parse_file_header(line);

    switch (parser->reg_version)
    {
    case REG_VERSION_31:
        set_state(parser, PARSE_WIN31_LINE);
        break;
    case REG_VERSION_40:
    case REG_VERSION_50:
        set_state(parser, LINE_START);
        break;
    default:
        get_line(NULL); /* Reset static variables */
        return NULL;
    }

    return line;
}

/* handler for parser PARSE_WIN31_LINE state */
static TCHAR *parse_win31_line_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line, *value;
    static TCHAR hkcr[] = _T("HKEY_CLASSES_ROOT");
    unsigned int key_end = 0;

    if (!(line = get_line(parser->file)))
        return NULL;

    if (_tcsncmp(line, hkcr, _tcslen(hkcr)))
        return line;

    /* get key name */
    while (line[key_end] && !_istspace(line[key_end])) key_end++;

    value = line + key_end;
    while (*value == ' ' || *value == '\t') value++;

    if (*value == '=') value++;
    if (*value == ' ') value++; /* at most one space is skipped */

    line[key_end] = 0;

    if (open_key(parser, line) != ERROR_SUCCESS)
    {
        //output_message(STRING_OPEN_KEY_FAILED, line);
        return line;
    }

    parser->value_name = NULL;
    parser->data_type = REG_SZ;
    parser->data = value;
    parser->data_size = (_tcslen(value) + 1) * sizeof(TCHAR);

    set_state(parser, SET_VALUE);
    return value;
}

/* handler for parser LINE_START state */
static TCHAR *line_start_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line, *p;

    if (!(line = get_line(parser->file)))
        return NULL;

    for (p = line; *p; p++)
    {
        switch (*p)
        {
        case '[':
            set_state(parser, KEY_NAME);
            return p + 1;
        case '@':
            set_state(parser, DEFAULT_VALUE_NAME);
            return p;
        case '"':
            set_state(parser, QUOTED_VALUE_NAME);
            return p + 1;
        case ' ':
        case '\t':
            break;
        default:
            return p;
        }
    }

    return p;
}

/* handler for parser KEY_NAME state */
static TCHAR *key_name_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *p = pos, *key_end;

    if (*p == ' ' || *p == '\t' || !(key_end = _tcsrchr(p, ']')))
        goto done;

    *key_end = 0;

    if (*p == '-')
    {
        set_state(parser, DELETE_KEY);
        return p + 1;
    }
    else if (open_key(parser, p) != ERROR_SUCCESS)
    {
        //output_message(STRING_OPEN_KEY_FAILED, p);
    }

done:
    set_state(parser, LINE_START);
    return p;
}

/******************************************************************************
 * Removes the registry key with all subkeys. Parses full key name.
 *
 * Parameters:
 * reg_key_name - full name of registry branch to delete. Ignored if is NULL,
 *      empty, points to register key class, does not exist.
 */
void delete_registry_key(TCHAR *reg_key_name)
{
    TCHAR *key_name = NULL;
    HKEY key_class;

    if (!reg_key_name || !reg_key_name[0])
        return;

    if (!(key_class = parse_key_name(reg_key_name, &key_name)))
    {
        if (key_name) *(key_name - 1) = 0;
        return;
    }

    if (!key_name || !*key_name)
        return;
#ifdef _WIN32_WCE
    RegDeleteKey(key_class, key_name);
#else
    RegDeleteTree(key_class, key_name);
#endif
}

/* handler for parser DELETE_KEY state */
static TCHAR *delete_key_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *p = pos;

    close_key(parser);

    if (*p == 'H' || *p == 'h')
        delete_registry_key(p);

    set_state(parser, LINE_START);
    return p;
}

/* handler for parser DEFAULT_VALUE_NAME state */
static TCHAR *default_value_name_state(struct parser *parser, TCHAR *pos)
{
    free(parser->value_name);
    parser->value_name = NULL;

    set_state(parser, DATA_START);
    return pos + 1;
}

/* handler for parser QUOTED_VALUE_NAME state */
static TCHAR *quoted_value_name_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *val_name = pos, *p;

    free(parser->value_name);
    parser->value_name = NULL;

    if (!REGPROC_unescape_string(val_name, &p))
        goto invalid;

    /* copy the value name in case we need to parse multiple lines and the buffer is overwritten */
    parser->value_name = (TCHAR *)malloc((_tcslen(val_name) + 1) * sizeof(TCHAR));
    _tcscpy(parser->value_name, val_name);

    set_state(parser, DATA_START);
    return p;

invalid:
    set_state(parser, LINE_START);
    return val_name;
}

/* handler for parser DATA_START state */
static TCHAR *data_start_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *p = pos;
    unsigned int len;

    while (*p == ' ' || *p == '\t') p++;
    if (*p != '=') goto done;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    /* trim trailing whitespace */
    len = _tcslen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
    p[len] = 0;

    if (*p == '-')
        set_state(parser, DELETE_VALUE);
    else
        set_state(parser, DATA_TYPE);
    return p;

done:
    set_state(parser, LINE_START);
    return p;
}

/* handler for parser DELETE_VALUE state */
static TCHAR *delete_value_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *p = pos + 1;

    while (*p == ' ' || *p == '\t') p++;
    if (*p && *p != ';') goto done;

    RegDeleteValue(parser->hkey, parser->value_name);

done:
    set_state(parser, LINE_START);
    return p;
}

/* handler for parser DATA_TYPE state */
static TCHAR *data_type_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line = pos;

    if (!parse_data_type(parser, &line))
    {
        set_state(parser, LINE_START);
        return line;
    }

    switch (parser->parse_type)
    {
    case REG_SZ:
        set_state(parser, STRING_DATA);
        break;
    case REG_DWORD:
        set_state(parser, DWORD_DATA);
        break;
    case REG_BINARY: /* all hex data types, including undefined */
        set_state(parser, HEX_DATA);
        break;
    default:
        set_state(parser, UNKNOWN_DATA);
    }

    return line;
}

/* handler for parser STRING_DATA state */
static TCHAR *string_data_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line;

    parser->data = pos;

	if (!REGPROC_unescape_string((TCHAR *)parser->data, &line))
        goto invalid;

    while (*line == ' ' || *line == '\t') line++;
    if (*line && *line != ';') goto invalid;

	parser->data_size = (_tcslen((TCHAR *)parser->data) + 1) * sizeof(TCHAR);

    set_state(parser, SET_VALUE);
    return line;

invalid:
    free_parser_data(parser);
    set_state(parser, LINE_START);
    return line;
}

/* handler for parser DWORD_DATA state */
static TCHAR *dword_data_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line = pos;

    parser->data = malloc(sizeof(DWORD));

    if (!convert_hex_to_dword(line, (DWORD *)parser->data))
        goto invalid;

    parser->data_size = sizeof(DWORD);

    set_state(parser, SET_VALUE);
    return line;

invalid:
    free_parser_data(parser);
    set_state(parser, LINE_START);
    return line;
}

/* handler for parser HEX_DATA state */
static TCHAR *hex_data_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line = pos;

    if (!*line)
        goto set_value;

    if (!convert_hex_csv_to_hex(parser, &line))
        goto invalid;

    if (parser->backslash)
    {
        set_state(parser, EOL_BACKSLASH);
        return line;
    }

    prepare_hex_string_data(parser);

set_value:
    set_state(parser, SET_VALUE);
    return line;

invalid:
    free_parser_data(parser);
    set_state(parser, LINE_START);
    return line;
}

/* handler for parser EOL_BACKSLASH state */
static TCHAR *eol_backslash_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *p = pos;

    while (*p == ' ' || *p == '\t') p++;
    if (*p && *p != ';') goto invalid;

    set_state(parser, HEX_MULTILINE);
    return pos;

invalid:
    free_parser_data(parser);
    set_state(parser, LINE_START);
    return p;
}

/* handler for parser HEX_MULTILINE state */
static TCHAR *hex_multiline_state(struct parser *parser, TCHAR *pos)
{
    TCHAR *line;

    if (!(line = get_line(parser->file)))
    {
        prepare_hex_string_data(parser);
        set_state(parser, SET_VALUE);
        return pos;
    }

    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == ';') return line;

    if (!iswxdigit(*line)) goto invalid;

    set_state(parser, HEX_DATA);
    return line;

invalid:
    free_parser_data(parser);
    set_state(parser, LINE_START);
    return line;
}

/* handler for parser UNKNOWN_DATA state */
static TCHAR *unknown_data_state(struct parser *parser, TCHAR *pos)
{
    //output_message(STRING_UNKNOWN_DATA_FORMAT, parser->data_type);

    set_state(parser, LINE_START);
    return pos;
}

/* handler for parser SET_VALUE state */
static TCHAR *set_value_state(struct parser *parser, TCHAR *pos)
{
    RegSetValueEx(parser->hkey, parser->value_name, 0, parser->data_type,
                  (BYTE *)parser->data, parser->data_size);

    free_parser_data(parser);

    if (parser->reg_version == REG_VERSION_31)
        set_state(parser, PARSE_WIN31_LINE);
    else
        set_state(parser, LINE_START);

    return pos;
}

static TCHAR *get_lineA(FILE *fp)
{
    static TCHAR *lineW;
    static size_t size;
    static char *buf, *next;
    char *line;

    free(lineW);

    if (!fp) goto cleanup;

    if (!size)
    {
        size = REG_VAL_BUF_SIZE;
        buf = (char *)malloc(size);
        *buf = 0;
        next = buf;
    }
    line = next;

    while (next)
    {
        char *p = strpbrk(line, "\r\n");
        if (!p)
        {
            size_t len, count;
            len = strlen(next);
            memmove(buf, next, len + 1);
            if (size - len < 3)
            {
                size *= 2;
                buf = (char *)realloc(buf, size);
            }
            if (!(count = fread(buf + len, 1, size - len - 1, fp)))
            {
                next = NULL;
#ifdef _UNICODE
                lineW = GetWideString(buf);
#else
                lineW = buf;
#endif
                return lineW;
            }
            buf[len + count] = 0;
            next = buf;
            line = buf;
            continue;
        }
        next = p + 1;
        if (*p == '\r' && *(p + 1) == '\n') next++;
        *p = 0;
#ifdef _UNICODE
        lineW = GetWideString(line);
#else
        lineW = line;
#endif
        return lineW;
    }

cleanup:
    lineW = NULL;
    if (size) free(buf);
    size = 0;
    return NULL;
}

static TCHAR *get_lineW(FILE *fp)
{
    static TCHAR *lineA;
    static size_t size;
    static WCHAR *buf, *next;
    WCHAR *line;

    if (!fp) goto cleanup;

    if (!size)
    {
        size = REG_VAL_BUF_SIZE;
        buf = (WCHAR *)malloc(size * sizeof(WCHAR));
        *buf = 0;
        next = buf;
    }
    line = next;

    while (next)
    {
        WCHAR *p = wcspbrk(line, L"\r\n");
        if (!p)
        {
            size_t len, count;
            len = wcslen(next);
            memmove(buf, next, (len + 1) * sizeof(WCHAR));
            if (size - len < 3)
            {
                size *= 2;
                buf = (WCHAR *)realloc(buf, size * sizeof(WCHAR));
            }
            if (!(count = fread(buf + len, sizeof(WCHAR), size - len - 1, fp)))
            {
                next = NULL;
#ifdef _UNICODE
                lineA = buf;
#else
                lineA = GetMultiByteString(buf);
#endif
                return lineA;
            }
            buf[len + count] = 0;
            next = buf;
            line = buf;
            continue;
        }
        next = p + 1;
        if (*p == '\r' && *(p + 1) == '\n') next++;
        *p = 0;
#ifdef _UNICODE
        lineA = line;
#else
        lineA = GetMultiByteString(line);
#endif
        return lineA;
    }

cleanup:
    if (size) free(buf);
    size = 0;
    return NULL;
}

/******************************************************************************
 * Reads contents of the specified file into the registry.
 */
BOOL import_registry_file(FILE *reg_file)
{
    BYTE s[2];
    struct parser parser;
    TCHAR *pos;

    if (!reg_file || (fread(s, 2, 1, reg_file) != 1))
        return FALSE;

    parser.is_unicode = (s[0] == 0xff && s[1] == 0xfe);
    get_line = parser.is_unicode ? get_lineW : get_lineA;

    parser.file          = reg_file;
    parser.two_wchars[0] = s[0];
    parser.two_wchars[1] = s[1];
    parser.reg_version   = -1;
    parser.hkey          = NULL;
    parser.key_name      = NULL;
    parser.value_name    = NULL;
    parser.parse_type    = 0;
    parser.data_type     = 0;
    parser.data          = NULL;
    parser.data_size     = 0;
    parser.backslash     = FALSE;
    parser.state         = HEADER;

    pos = parser.two_wchars;

    /* parser main loop */
    while (pos)
        pos = (parser_funcs[parser.state])(&parser, pos);

    if (parser.reg_version == REG_VERSION_FUZZY || parser.reg_version == REG_VERSION_INVALID)
        return parser.reg_version == REG_VERSION_FUZZY;

    free(parser.value_name);
    close_key(&parser);

    return TRUE;
}

static void REGPROC_write_line(FILE *fp, const TCHAR *str, BOOL unicode)
{
#ifdef _UNICODE
    if (unicode)
        fwrite(str, sizeof(TCHAR), _tcslen(str), fp);
    else
    {
        char *strA = GetMultiByteString(str);
        fwrite(strA, sizeof *strA, strlen(strA), fp);
        free(strA);
    }
#else
    if (unicode)
    {
        WCHAR *strW = GetWideString(str);
        fwrite(strW, sizeof *strW, wcslen(strW), fp);
        free(strW);
    }
    else
        fwrite(str, sizeof *str, _tcslen(str), fp);
#endif
}

static TCHAR *REGPROC_escape_string(TCHAR *str, size_t str_len, size_t *line_len)
{
    size_t i, escape_count, pos;
    TCHAR *buf;

    for (i = 0, escape_count = 0; i < str_len; i++)
    {
        TCHAR c = str[i];

        if (!c) break;

        if (c == '\r' || c == '\n' || c == '\\' || c == '"')
            escape_count++;
    }

    buf = (TCHAR *)malloc((str_len + escape_count + 1) * sizeof(TCHAR));

    for (i = 0, pos = 0; i < str_len; i++, pos++)
    {
        TCHAR c = str[i];

        if (!c) break;

        switch (c)
        {
        case '\r':
            buf[pos++] = '\\';
            buf[pos] = 'r';
            break;
        case '\n':
            buf[pos++] = '\\';
            buf[pos] = 'n';
            break;
        case '\\':
            buf[pos++] = '\\';
            buf[pos] = '\\';
            break;
        case '"':
            buf[pos++] = '\\';
            buf[pos] = '"';
            break;
        default:
            buf[pos] = c;
        }
    }

    buf[pos] = 0;
    *line_len = pos;
    return buf;
}

static size_t export_value_name(FILE *fp, TCHAR *name, size_t len, BOOL unicode)
{
    static const TCHAR default_name[] = _T("@=");
    size_t line_len;

    if (name && *name)
    {
        TCHAR *str = REGPROC_escape_string(name, len, &line_len);
        TCHAR *buf = (TCHAR *)malloc((line_len + 4) * sizeof(TCHAR));
        line_len = _stprintf(buf, _T("\"%s\"="), str);
        REGPROC_write_line(fp, buf, unicode);
        free(buf);
        free(str);
    }
    else
    {
        line_len = _tcslen(default_name);
        REGPROC_write_line(fp, default_name, unicode);
    }

    return line_len;
}

static void export_string_data(TCHAR **buf, TCHAR *data, size_t size)
{
    size_t len = 0, line_len;
    TCHAR *str;

    if (size)
        len = size / sizeof(TCHAR) - 1;
    str = REGPROC_escape_string(data, len, &line_len);
	*buf = (TCHAR *)malloc((line_len + 3) * sizeof(TCHAR));
    _stprintf(*buf, _T("\"%s\""), str);
    free(str);
}

static void export_dword_data(TCHAR **buf, DWORD *data)
{
    *buf = (TCHAR *)malloc(15 * sizeof(TCHAR));
    _stprintf(*buf, _T("dword:%08x"), *data);
}

static size_t export_hex_data_type(FILE *fp, DWORD type, BOOL unicode)
{
    static const TCHAR hex[] = _T("hex:");
    size_t line_len;

    if (type == REG_BINARY)
    {
        line_len = _tcslen(hex);
        REGPROC_write_line(fp, hex, unicode);
    }
    else
    {
        TCHAR *buf = (TCHAR *)malloc(15 * sizeof(TCHAR));
        line_len = _stprintf(buf, _T("hex(%x):"), type);
        REGPROC_write_line(fp, buf, unicode);
        free(buf);
    }

    return line_len;
}

#define MAX_HEX_CHARS 77

static void export_hex_data(FILE *fp, TCHAR **buf, DWORD type, DWORD line_len,
                            void *data, DWORD size, BOOL unicode)
{
    size_t num_commas, i, pos;

    line_len += export_hex_data_type(fp, type, unicode);

    if (!size) return;
#ifdef _UNICODE
    if (!unicode && (type == REG_EXPAND_SZ || type == REG_MULTI_SZ))
        data = GetMultiByteStringN((WCHAR *)data, size / sizeof(WCHAR), &size);
#endif
    num_commas = size - 1;
    *buf = (TCHAR *)malloc(size * 3 * sizeof(TCHAR));

    for (i = 0, pos = 0; i < size; i++)
    {
        pos += _stprintf(*buf + pos, _T("%02x"), ((BYTE *)data)[i]);
        if (i == num_commas) break;
        (*buf)[pos++] = ',';
        (*buf)[pos] = 0;
        line_len += 3;

        if (line_len >= MAX_HEX_CHARS)
        {
            REGPROC_write_line(fp, *buf, unicode);
            REGPROC_write_line(fp, _T("\\\r\n  "), unicode);
            line_len = 2;
            pos = 0;
        }
    }
}

static void export_newline(FILE *fp, BOOL unicode)
{
    REGPROC_write_line(fp, _T("\r\n"), unicode);
}

static void export_data(FILE *fp, TCHAR *value_name, DWORD value_len, DWORD type,
                        void *data, size_t size, BOOL unicode)
{
    TCHAR *buf = NULL;
    size_t line_len = export_value_name(fp, value_name, value_len, unicode);

    switch (type)
    {
    case REG_SZ:
        export_string_data(&buf, (TCHAR *)data, size);
        break;
    case REG_DWORD:
        if (size)
        {
            export_dword_data(&buf, (DWORD *)data);
            break;
        }
        /* fall through */
    case REG_NONE:
    case REG_EXPAND_SZ:
    case REG_BINARY:
    case REG_MULTI_SZ:
    default:
        export_hex_data(fp, &buf, type, line_len, data, size, unicode);
        break;
    }

    if (size || type == REG_SZ)
    {
        REGPROC_write_line(fp, buf, unicode);
        free(buf);
    }

    export_newline(fp, unicode);
}

static TCHAR *build_subkey_path(TCHAR *path, DWORD path_len, TCHAR *subkey_name, DWORD subkey_len)
{
    TCHAR *subkey_path;

    subkey_path = (TCHAR *)malloc((path_len + subkey_len + 2) * sizeof(TCHAR));
    _stprintf(subkey_path, _T("%s\\%s"), path, subkey_name);

    return subkey_path;
}

static void export_key_name(FILE *fp, TCHAR *name, BOOL unicode)
{
    TCHAR *buf;

    buf = (TCHAR *)malloc((_tcslen(name) + 7) * sizeof(TCHAR));
    _stprintf(buf, _T("\r\n[%s]\r\n"), name);
    REGPROC_write_line(fp, buf, unicode);
    free(buf);
}

#define MAX_SUBKEY_LEN   257

static int export_registry_data(FILE *fp, HKEY key, TCHAR *path, BOOL unicode)
{
    LONG rc;
    DWORD max_value_len = 256, value_len;
    DWORD max_data_bytes = 2048, data_size;
    DWORD subkey_len;
    DWORD i, type, path_len;
    TCHAR *value_name, *subkey_name, *subkey_path;
    BYTE *data;
    HKEY subkey;

    export_key_name(fp, path, unicode);

	value_name = (TCHAR *)malloc(max_value_len * sizeof(TCHAR));
	data = (BYTE *)malloc(max_data_bytes);

    i = 0;
    for (;;)
    {
        value_len = max_value_len;
        data_size = max_data_bytes;
        rc = RegEnumValue(key, i, value_name, &value_len, NULL, &type, data, &data_size);
        if (rc == ERROR_SUCCESS)
        {
            export_data(fp, value_name, value_len, type, data, data_size, unicode);
            i++;
        }
        else if (rc == ERROR_MORE_DATA)
        {
            if (data_size > max_data_bytes)
            {
                max_data_bytes = data_size;
                data = (BYTE *)realloc(data, max_data_bytes);
            }
            else
            {
                max_value_len *= 2;
                value_name = (TCHAR *)realloc(value_name, max_value_len * sizeof(TCHAR));
            }
        }
        else break;
    }

    free(data);
    free(value_name);

    subkey_name = (TCHAR *)malloc(MAX_SUBKEY_LEN * sizeof(TCHAR));

    path_len = _tcslen(path);

    i = 0;
    for (;;)
    {
        subkey_len = MAX_SUBKEY_LEN;
        rc = RegEnumKeyEx(key, i, subkey_name, &subkey_len, NULL, NULL, NULL, NULL);
        if (rc == ERROR_SUCCESS)
        {
            subkey_path = build_subkey_path(path, path_len, subkey_name, subkey_len);
            if (!RegOpenKeyEx(key, subkey_name, 0, KEY_READ, &subkey))
            {
                export_registry_data(fp, subkey, subkey_path, unicode);
                RegCloseKey(subkey);
            }
            free(subkey_path);
            i++;
        }
        else break;
    }

    free(subkey_name);
    return 0;
}

static FILE *REGPROC_open_export_file(const TCHAR *file_name, BOOL unicode)
{
    FILE *file;

    file = _tfopen(file_name, _T("wb"));
    if (file)
    {
        if (unicode)
        {
            static const WCHAR header[] = L"\xFEFFWindows Registry Editor Version 5.00\r\n";
            fwrite(header, sizeof *header, wcslen(header), file);
        }
        else
            fputs("REGEDIT4\r\n", file);
    }
    return file;
}

static HKEY open_export_key(HKEY key_class, TCHAR *subkey, TCHAR *path)
{
    HKEY key;

    if (RegOpenKeyEx(key_class, subkey, 0, KEY_READ, &key) == ERROR_SUCCESS)
        return key;

    return NULL;
}

static BOOL export_key(const TCHAR *file_name, TCHAR *path, BOOL unicode)
{
    HKEY key_class, key;
    TCHAR *subkey;
    FILE *fp;

    if (!(key_class = parse_key_name(path, &subkey)))
    {
        if (subkey) *(subkey - 1) = 0;
        return FALSE;
    }

    if (!(key = open_export_key(key_class, subkey, path)))
        return FALSE;

    fp = REGPROC_open_export_file(file_name, unicode);
    export_registry_data(fp, key, path, unicode);
    export_newline(fp, unicode);
    fclose(fp);

    RegCloseKey(key);
    return TRUE;
}

static BOOL export_all(const TCHAR *file_name, TCHAR *path, BOOL unicode)
{
    FILE *fp;
    int i;
    static const HKEY classes[] = { HKEY_LOCAL_MACHINE, HKEY_USERS };
    HKEY key;
    TCHAR *class_name;

    fp = REGPROC_open_export_file(file_name, unicode);

    for (i = 0; i < _countof(classes); i++)
    {
        if (!(key = open_export_key(classes[i], NULL, path)))
        {
            fclose(fp);
            return FALSE;
        }

        class_name = (TCHAR *)malloc((_tcslen(reg_class_names[i]) + 1) * sizeof(TCHAR));
        _tcscpy(class_name, reg_class_names[i]);

        export_registry_data(fp, classes[i], class_name, unicode);

        free(class_name);
        RegCloseKey(key);
    }

    export_newline(fp, unicode);
    fclose(fp);

    return TRUE;
}

BOOL export_registry_key(const TCHAR *file_name, TCHAR *path, BOOL unicode)
{
    if (path && *path)
        return export_key(file_name, path, unicode);
    else
        return export_all(file_name, path, unicode);
}
