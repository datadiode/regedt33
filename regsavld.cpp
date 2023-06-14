#include "regedit.h"
#include "resource.h"

#include "regsavld.h"

#ifndef WINCOMMDLGAPI
/*
 * File Open/Save Dialog Constants (from wvtwin.ch)
 */
#define OFN_READONLY                              1
#define OFN_OVERWRITEPROMPT                       2
#define OFN_HIDEREADONLY                          4
#define OFN_NOCHANGEDIR                           8
#define OFN_SHOWHELP                              16
#define OFN_ENABLEHOOK                            32
#define OFN_ENABLETEMPLATE                        64
#define OFN_ENABLETEMPLATEHANDLE                  128
#define OFN_NOVALIDATE                            256
#define OFN_ALLOWMULTISELECT                      512
#define OFN_EXTENSIONDIFFERENT                    1024
#define OFN_PATHMUSTEXIST                         2048
#define OFN_FILEMUSTEXIST                         4096
#define OFN_CREATEPROMPT                          8192
#define OFN_SHAREAWARE                            16384
#define OFN_NOREADONLYRETURN                      32768
#define OFN_NOTESTFILECREATE                      65536
#define OFN_NONETWORKBUTTON                       131072
#define OFN_NOLONGNAMES                           262144              // force no long names for 4.x modules
#define OFN_EXPLORER                              524288              // new look commdlg
#define OFN_NODEREFERENCELINKS                    1048576
#define OFN_LONGNAMES                             2097152             // force long names for 3.x modules
#define OFN_ENABLEINCLUDENOTIFY                   4194304             // send include message to callback
#define OFN_ENABLESIZING                          8388608
#define OFN_DONTADDTORECENT                       33554432
#define OFN_FORCESHOWHIDDEN                       268435456           // Show All files including System and hidden files
/*
 * File Open/Save Dialog DLL Imports
 */
#define GetOpenFileName GetOpenFileNameW
#define GetSaveFileName GetSaveFileNameW
#endif

static struct commdlg {
  struct OPENFILENAME {
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCTSTR lpstrFilter;
    LPTSTR lpstrCustomFilter;
    DWORD nMaxCustFilter;
    DWORD nFilterIndex;
    LPTSTR lpstrFile;
    DWORD nMaxFile;
    LPTSTR lpstrFileTitle;
    DWORD nMaxFileTitle;
    LPTSTR lpstrInitialDir;
    LPCTSTR lpstrTitle;
    DWORD Flags;
    WORD nFileOffset;
    WORD nFileExtension;
    LPCTSTR lpstrDefExt;
    DWORD lCustData;
    UINT_PTR(CALLBACK*lpfnHook)(HWND, UINT, WPARAM, LPARAM);
    LPCTSTR lpTemplateName;
  };
  HMODULE h;
  DllImport<BOOL(APIENTRY*)(OPENFILENAME *)> GetOpenFileName;
  DllImport<BOOL(APIENTRY*)(OPENFILENAME *)> GetSaveFileName;
} const commdlg = {
#ifdef _WIN32_WCE
  GetModuleHandle(_T("COREDLL.DLL")),
#else
  LoadLibrary(_T("COMDLG32.DLL")),
#endif
  GetProcAddressA(commdlg.h, _CRT_STRINGIZE(GetOpenFileName)),
  GetProcAddressA(commdlg.h, _CRT_STRINGIZE(GetSaveFileName)),
};

BOOL import_registry_file(FILE *reg_file);
BOOL export_registry_key(const TCHAR *file_name, TCHAR *path, BOOL unicode);

INT_PTR CALLBACK DialogLDH(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    load_hive_dialog_data *dd;
    switch (msg) {
    case WM_INITDIALOG: 
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      dd = (load_hive_dialog_data*)lParam;
      SetDlgItemText(hwnd, IDC_E_KEYNAME, dd->root_key_name);
      SetEditCtrlHistAndText(hwnd, IDC_E_FILENAME, dd->fname.c? dd->fname.c : _T(""));
      SetDlgEditCtrlHist(hwnd, IDC_E_SUBKEYNAME);
#ifndef _WIN32_WCE
      SetDlgItemText(hwnd, IDC_SEWARN, has_rest_priv? _T("You have \"") SE_RESTORE_NAME _T("\"") : _T("You don't have \"") SE_RESTORE_NAME _T("\""));
#endif
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
      dd = (load_hive_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
      switch (LOWORD (wParam)) {
      case IDCANCEL: EndDialog(hwnd,0); break; 
      case IDOK: 
        dd->fname.GetDlgItemText(hwnd, IDC_E_FILENAME);
        dd->subkey_name.GetDlgItemText(hwnd, IDC_E_SUBKEYNAME);
        if (!dd->subkey_name.size()) {
          SetDlgItemText(hwnd, IDC_E_SUBKEYNAME, FindLastComponent(dd->fname.c));
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (DisplayOFNdlg(tname, _T("Choose registry file"), 0)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
#ifndef _WIN32_WCE
      case IDC_B_GETPRIV: 
        {
          TCHAR *c = (TCHAR *)_tcschr(dd->root_key_name, ':');
          if (c) *c = 0;
          if (!EnablePrivilege_NT(c? dd->root_key_name : 0, SE_RESTORE_NAME)) {
            if (!c) has_rest_priv = true;
            TCHAR msgbuf[100] = _T("Enabled \"") SE_RESTORE_NAME _T("\"");
            if (c) _tcscat(msgbuf, _T(" on ")), _tcscat(msgbuf, dd->root_key_name);
            SetDlgItemText(hwnd, IDC_SEWARN, msgbuf);
          }
          if (c) *c = ':';
        }
        break;
#endif
      }
      return 1;
    }
    return 0;
}

INT_PTR CALLBACK DialogSVK(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  save_key_dialog_data *dd;
  switch (msg) {
    case WM_INITDIALOG: 
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      dd = (save_key_dialog_data*)lParam;
      SetEditCtrlHistAndText(hwnd, IDC_E_KEYNAME, dd->key_name.c);
      SetEditCtrlHistAndText(hwnd, IDC_E_FILENAME, dd->fname.c? dd->fname.c : _T(""));
#ifndef _WIN32_WCE
      SetDlgItemText(hwnd, IDC_SEWARN, has_back_priv? _T("You have \"") SE_BACKUP_NAME _T("\"") : _T("You don't have \"") SE_BACKUP_NAME _T("\""));
#endif
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
      dd = (save_key_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
      switch (LOWORD (wParam)) {
      case IDCANCEL: EndDialog(hwnd,0); break; 
      case IDOK: 
        dd->fname.GetDlgItemText(hwnd, IDC_E_FILENAME);
        dd->key_name.GetDlgItemText(hwnd, IDC_E_KEYNAME);
        if (!dd->fname.size()) {
          SetDlgItemText(hwnd, IDC_E_FILENAME, FindLastComponent(dd->key_name.c));
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (DisplayOFNdlg(tname, _T("Choose save file"), 0, true, true)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
#ifndef _WIN32_WCE
      case IDC_B_GETPRIV: 
        {
          TCHAR *c = _tcschr(dd->key_name.c, ':'), *cc = _tcschr(dd->key_name.c, '\\');
          if (c > cc) c = 0;
          if (c) *c = 0;
          if (!EnablePrivilege_NT(c? dd->key_name.c : 0, SE_BACKUP_NAME)) {
            if (!c) has_back_priv = true;
            TCHAR msgbuf[100] = _T("Enabled \"") SE_BACKUP_NAME _T("\"");
            if (c) _tcscat(msgbuf, _T(" on ")), _tcscat(msgbuf, dd->key_name.c);
            SetDlgItemText(hwnd, IDC_SEWARN, msgbuf);
          }
          if (c) *c = ':';
        }
        break;
#endif
      }
      return 1;
    }
    return 0;
}

INT_PTR CALLBACK DialogLDK(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  load_key_dialog_data *dd;
  switch (msg) {
	case WM_INITDIALOG: 
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      dd = (load_key_dialog_data*)lParam;
      SetEditCtrlHistAndText(hwnd, IDC_E_KEYNAME, dd->key_name.c);
      SetEditCtrlHistAndText(hwnd, IDC_E_FILENAME, dd->fname.c? dd->fname.c : _T(""));
      SendDlgItemMessage(hwnd, IDC_CHK_FORCE,   BM_SETCHECK, dd->force, 0);
      SendDlgItemMessage(hwnd, IDC_CHK_NOLAZY , BM_SETCHECK, dd->nolazy, 0);
      SendDlgItemMessage(hwnd, IDC_CHK_REFRESH, BM_SETCHECK, dd->refresh, 0);
      SendDlgItemMessage(hwnd, IDC_CHK_HIVEVOL, BM_SETCHECK, dd->volatil, 0);
#ifndef _WIN32_WCE
      SetDlgItemText(hwnd, IDC_SEWARN, has_rest_priv? _T("You have \"") SE_RESTORE_NAME _T("\"") : _T("You don't have \"") SE_RESTORE_NAME _T("\""));
#endif
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
      dd = (load_key_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
      switch (LOWORD (wParam)) {
      case IDCANCEL: EndDialog(hwnd,0); break; 
      case IDOK: 
        dd->fname.GetDlgItemText(hwnd, IDC_E_FILENAME);
        dd->key_name.GetDlgItemText(hwnd, IDC_E_KEYNAME);
        dd->force  =  SendDlgItemMessage(hwnd, IDC_CHK_FORCE, BM_GETCHECK, 0, 0) != 0;
        dd->nolazy  = SendDlgItemMessage(hwnd, IDC_CHK_NOLAZY, BM_GETCHECK, 0, 0) != 0;
        dd->refresh = SendDlgItemMessage(hwnd, IDC_CHK_REFRESH, BM_GETCHECK, 0, 0) != 0;
        dd->volatil = SendDlgItemMessage(hwnd, IDC_CHK_HIVEVOL, BM_GETCHECK, 0, 0) != 0;
        if (!dd->fname.size()) {
          SetDlgItemText(hwnd, IDC_E_FILENAME, FindLastComponent(dd->key_name.c));
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (DisplayOFNdlg(tname, _T("Load key from file"), 0, false, false)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
#ifndef _WIN32_WCE
      case IDC_B_GETPRIV: 
        {
          TCHAR *c = _tcschr(dd->key_name.c, ':'), *cc = _tcschr(dd->key_name.c, '\\');
          if (c > cc) c = 0;
          if (c) *c = 0;
          if (!EnablePrivilege_NT(c? dd->key_name.c : 0, SE_RESTORE_NAME)) {
            if (!c) has_rest_priv = true;
            TCHAR msgbuf[100] = _T("Enabled \"") SE_RESTORE_NAME _T("\"");
            if (c) _tcscat(msgbuf, _T(" on ")), _tcscat(msgbuf, dd->key_name.c);
            SetDlgItemText(hwnd, IDC_SEWARN, msgbuf);
          }
          if (c) *c = ':';
        }
        break;
#endif
      }
      return 1;
    }
    return 0;
}


INT_PTR CALLBACK DialogRplK(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  replace_key_dialog_data *dd;
  switch (msg) {
    case WM_INITDIALOG: 
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      dd = (replace_key_dialog_data*)lParam;
      SetEditCtrlHistAndText(hwnd, IDC_E_KEYNAME, dd->key_name.c);
      SetEditCtrlHistAndText(hwnd, IDC_E_FILENAME, dd->fname_new.c? dd->fname_new.c : _T(""));
      SetEditCtrlHistAndText(hwnd, IDC_E_FILENAME2, dd->fname_old.c? dd->fname_old.c : _T(""));
#ifndef _WIN32_WCE
      SetDlgItemText(hwnd, IDC_SEWARN, has_rest_priv? _T("You have \"") SE_RESTORE_NAME _T("\"") : _T("You don't have \"") SE_RESTORE_NAME _T("\""));
#endif
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
      dd = (replace_key_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
      switch (LOWORD (wParam)) {
      case IDCANCEL: EndDialog(hwnd,0); break; 
      case IDOK: 
        dd->fname_new.GetDlgItemText(hwnd, IDC_E_FILENAME);
        dd->fname_old.GetDlgItemText(hwnd, IDC_E_FILENAME2);
        dd->key_name.GetDlgItemText(hwnd, IDC_E_KEYNAME);
        if (!dd->fname_new.size()) {
          SetDlgItemText(hwnd, IDC_E_FILENAME, FindLastComponent(dd->key_name.c));
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (DisplayOFNdlg(tname, _T("Choose file with new key data"), 0, true, false)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
      case IDC_B_CHOOSE2:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME2);
          if (DisplayOFNdlg(tname, _T("Choose file to save old key data to"), 0, true, true)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME2, tname.c? tname.c : _T(""));
          }
        }
        break;
#ifndef _WIN32_WCE
      case IDC_B_GETPRIV: 
        {
          TCHAR *c = _tcschr(dd->key_name.c, ':'), *cc = _tcschr(dd->key_name.c, '\\');
          if (c > cc) c = 0;
          if (c) *c = 0;
          if (!EnablePrivilege_NT(c? dd->key_name.c : 0, SE_RESTORE_NAME)) {
            if (!c) has_rest_priv = true;
            TCHAR msgbuf[100] = _T("Enabled \"") SE_RESTORE_NAME _T("\"");
            if (c) _tcscat(msgbuf, _T(" on ")), _tcscat(msgbuf, dd->key_name.c);
            SetDlgItemText(hwnd, IDC_SEWARN, msgbuf);
          }
          if (c) *c = ':';
        }
        break;
#endif
      }
      return 1;
    }
    return 0;
}

int DisplayOFNdlg(achar &name, const TCHAR *title, const TCHAR *filter, bool no_RO, bool for_save) {
  if (name.s < 4096) name.resize(4096);
  commdlg::OPENFILENAME ofn;
  ofn.lStructSize = sizeof ofn;
  ofn.hwndOwner = MainWindow;
  ofn.hInstance = NULL;
  ofn.lpstrFilter = filter;//"Executable files\0*.exe;*.bat;*.com\0All files\0*.*\0";
  ofn.lpstrCustomFilter = NULL;
  ofn.nMaxCustFilter = 0;
  ofn.nFilterIndex = 1;
  ofn.lpstrFile = name.c;
  ofn.nMaxFile = name.s;
  ofn.lpstrFileTitle = NULL; 
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = title; //"Choose a file to open";
  ofn.Flags = 0;
  if (no_RO) ofn.Flags |= OFN_HIDEREADONLY;
  ofn.Flags |= for_save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST;
  ofn.nFileOffset = 0;
  ofn.nFileExtension = 0;
  // Guess default filenam extension from first filter (based on specific assumptions which don't hold in other contexts)
  ofn.lpstrDefExt = ofn.lpstrFilter ? ofn.lpstrFilter + _tcslen(ofn.lpstrFilter) + 3 : NULL;
  ofn.lCustData = 0;
  ofn.lpfnHook = NULL;
  ofn.lpTemplateName = NULL;
  if (!*commdlg.GetSaveFileName || !*commdlg.GetOpenFileName) {
    MessageBox(MainWindow, _T("Unsupported on this platform"), _T("DisplayOFNdlg"), MB_ICONSTOP);
    return 0;
  }
  if ((for_save ? *commdlg.GetSaveFileName : *commdlg.GetOpenFileName)(&ofn)) {
    name.checklen();
	return ofn.nFilterIndex ? ofn.nFilterIndex : 1;
  }
#ifndef _WIN32_WCE
  DWORD err = CommDlgExtendedError();
  if (err) CommDlgErrMsgDlgBox(_T("DisplayOFNdlg"), err);
#endif
  return 0;
}

int LoadDump(const TCHAR *fname) {
  FILE *f = _tfopen(fname, _T("rb"));
  if (!import_registry_file(f)) {
    MessageBox(MainWindow, fname, _T("Could not read file"), MB_ICONERROR);
    return 1;
  }
  return 0;
}

int SaveDump(const TCHAR *fname, TCHAR *key, BOOL unicode) {
  if (!export_registry_key(fname, key, unicode)) {
    MessageBox(MainWindow, fname, _T("Could not write file"), MB_ICONERROR);
    return 1;
  }
  return 0;
}
