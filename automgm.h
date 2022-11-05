#ifndef __AUTOMGM_H__
#define __AUTOMGM_H__
#pragma once

/// Simple useful classes for dealing with allocated character strings
struct achar {
  union { BYTE *b; TCHAR *c; };
  DWORD s, l;
  achar() { c = 0; s = 0; l = 0; }
  achar(DWORD n) { c = (TCHAR*)malloc(s = n + sizeof(*c)); l = 0; }
  achar(const achar &a) { s = a.s, l = a.l; c = a.c? (TCHAR*)malloc(s) : 0; c && memcpy(c, a.c, s); }
  void operator=(achar a) { swap(a); }
  achar(const TCHAR *ss) { c = 0; if (ss) _tcscpy(resize(l = _tcslen(ss) * sizeof(TCHAR)), ss); else s = l = 0; }
  ~achar() { free(c); }
  TCHAR *resize(DWORD n) { c = (TCHAR*)realloc(c, s = n + sizeof(TCHAR)); return c; }
  TCHAR *resize() { return l >= s? resize(l) : c; }
  void checklen() { l = c? _tcslen(c) * sizeof(TCHAR) : 0; }
  DWORD size() const { return l; }
  void swap(achar &a) { TCHAR *t = c; c = a.c; a.c = t; DWORD i = s; s = a.s; a.s = i; i = l; l = a.l; a.l = i; }
  void strlwr() { for(TCHAR *s = c, *e = c + l / sizeof(TCHAR); s < e; s++) *s = _totlower(*s); }
  UINT GetDlgItemText(HWND hwnd, int nIDDlgItem);
  UINT GetDlgItemTextUnCEsc(HWND hwnd, int nIDDlgItem);
  LONG QueryValue(HKEY hk, const TCHAR *name, DWORD &type);
};

struct fchar {
  TCHAR *c;
  fchar() : c(0) {}
  explicit fchar(void *v) : c((TCHAR*)v) {}
  ~fchar() { free(c); }
private:
  fchar(const fchar &f);
};


struct auto_close_handle {
  HANDLE h;
  auto_close_handle(HANDLE H) : h(H) {}
  ~auto_close_handle() { CloseHandle(h); }
};

struct auto_close_hkey {
  HKEY hk;
  auto_close_hkey(HKEY HK) : hk(HK) {}
  ~auto_close_hkey() { if (hk != (HKEY)INVALID_HANDLE_VALUE) RegCloseKey(hk); }
};

#endif //__AUTOMGM_H__
