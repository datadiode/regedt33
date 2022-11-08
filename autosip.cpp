// Silly WinCE SIP automator
// Copyright (c) 2022 datadiode
// SPDX-License-Identifier: MIT
#include <windows.h>
#include <sipapi.h>

template<typename f>
struct DllImport {
  FARPROC p;
  f operator*() const { return reinterpret_cast<f>(p); }
};

static struct sipapi {
  static VOID CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD) {
    static HWND static_focus = NULL;
    static DWORD static_flag = SIPF_OFF;
    HWND const focus = GetFocus();
    DWORD const flag = HideCaret(NULL) && ShowCaret(NULL) ? SIPF_ON : SIPF_OFF;
    if (static_focus != focus || static_flag != flag) {
      static_focus = focus;
      (*::sipapi.SipShowIM)(static_flag = flag);
    }
  }
  HMODULE h;
  DllImport<BOOL(WINAPI*)(DWORD)> SipShowIM;
  UINT_PTR timer;
} const sipapi = {
  GetModuleHandle(L"COREDLL.DLL"),
  GetProcAddress(sipapi.h, L"SipShowIM"),
  SetTimer(NULL, 0, 500, sipapi::TimerProc)
};
