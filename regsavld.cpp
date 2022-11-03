#include "regedit.h"
#include "resource.h"

#include "regsavld.h"

INT_PTR CALLBACK DialogLDH(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    load_hive_dialog_data *dd;
    switch (msg) {
	case WM_INITDIALOG: 
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      dd = (load_hive_dialog_data*)lParam;
      SetDlgItemText(hwnd, IDC_E_KEYNAME, dd->root_key_name);
      SetEditCtrlHistAndText(hwnd, IDC_E_FILENAME, dd->fname.c? dd->fname.c : _T(""));
      SetDlgEditCtrlHist(hwnd, IDC_E_SUBKEYNAME);
      //if (win9x) {
      //  EnableWindow(GetDlgItem(hwnd, IDC_SEWARN), 0);
      //  EnableWindow(GetDlgItem(hwnd, IDC_B_GETPRIV), 0);
      //} else {
      SetDlgItemText(hwnd, IDC_SEWARN, has_rest_priv? _T("You have \"") SE_RESTORE_NAME _T("\"") : _T("You don't have \"") SE_RESTORE_NAME _T("\""));
      //}
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
          TCHAR *c = _tcsrchr(dd->fname.c, '\\'); if (!c) c = dd->fname.c; else c++;
          SetDlgItemText(hwnd, IDC_E_SUBKEYNAME, c);
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (!DisplayOFNdlg(tname, _T("Choose registry file"), 0)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
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
      //if (win9x) {
      //  EnableWindow(GetDlgItem(hwnd, IDC_SEWARN), 0);
      //  EnableWindow(GetDlgItem(hwnd, IDC_B_GETPRIV), 0);
      //} else {
      SetDlgItemText(hwnd, IDC_SEWARN, has_back_priv? _T("You have \"") SE_BACKUP_NAME _T("\"") : _T("You don't have \"") SE_BACKUP_NAME _T("\""));
      //}
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
      dd = (save_key_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
	  switch (LOWORD (wParam)) {
	  case IDCANCEL: EndDialog(hwnd,0); break; 
	  case IDOK: 
        dd->fname.GetDlgItemText(hwnd, IDC_E_FILENAME);
        dd->key_name.GetDlgItemText(hwnd, IDC_E_KEYNAME);
        if (!dd->key_name.size()) {
          TCHAR *c = _tcsrchr(dd->key_name.c, '\\'); if (!c) c = dd->key_name.c; else c++;
          SetDlgItemText(hwnd, IDC_E_FILENAME, c);
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (!DisplayOFNdlg(tname, _T("Choose save file"), 0, true, true)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
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
      //if (win9x) {
      //  EnableWindow(GetDlgItem(hwnd, IDC_SEWARN), 0);
      //  EnableWindow(GetDlgItem(hwnd, IDC_B_GETPRIV), 0);
      //} else {
      SetDlgItemText(hwnd, IDC_SEWARN, has_rest_priv? _T("You have \"") SE_RESTORE_NAME _T("\"") : _T("You don't have \"") SE_RESTORE_NAME _T("\""));
      //}
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
        if (!dd->key_name.size()) {
          TCHAR *c = _tcsrchr(dd->key_name.c, '\\'); if (!c) c = dd->key_name.c; else c++;
          SetDlgItemText(hwnd, IDC_E_FILENAME, c);
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (!DisplayOFNdlg(tname, _T("Load key from file"), 0, false, false)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
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
      //if (win9x) {
      //  EnableWindow(GetDlgItem(hwnd, IDC_SEWARN), 0);
      //  EnableWindow(GetDlgItem(hwnd, IDC_B_GETPRIV), 0);
      //} else {
      SetDlgItemText(hwnd, IDC_SEWARN, has_rest_priv? _T("You have \"") SE_RESTORE_NAME _T("\"") : _T("You don't have \"") SE_RESTORE_NAME _T("\""));
      //}
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
        if (!dd->key_name.size()) {
          TCHAR *c = _tcsrchr(dd->key_name.c, '\\'); if (!c) c = dd->key_name.c; else c++;
          SetDlgItemText(hwnd, IDC_E_FILENAME, c);
          return 1;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_CHOOSE:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME);
          if (!DisplayOFNdlg(tname, _T("Choose file with new key data"), 0, true, false)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME, tname.c? tname.c : _T(""));
          }
        }
        break;
      case IDC_B_CHOOSE2:
        {
          achar tname;
          tname.GetDlgItemText(hwnd, IDC_E_FILENAME2);
          if (!DisplayOFNdlg(tname, _T("Choose file to save old key data to"), 0, true, true)) {
            SetDlgItemText(hwnd, IDC_E_FILENAME2, tname.c? tname.c : _T(""));
          }
        }
        break;
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
      }
	  return 1;
	}
	return 0;
}

int DisplayOFNdlg(achar &name, const TCHAR *title, const TCHAR *filter, bool no_RO, bool for_save) {
  if (name.s < 4096) name.resize(4096);
  OPENFILENAME ofn;
#ifdef OPENFILENAME_SIZE_VERSION_400
  ofn.lStructSize=OPENFILENAME_SIZE_VERSION_400; //????
#else
  ofn.lStructSize=sizeof(OPENFILENAME);
#endif
  ofn.hwndOwner=MainWindow;
  ofn.hInstance=NULL;
  ofn.lpstrFilter = filter;//"Executable files\0*.exe;*.bat;*.com\0All files\0*.*\0";
  ofn.lpstrCustomFilter=NULL;
  ofn.nMaxCustFilter=0;
  ofn.nFilterIndex=1;
  ofn.lpstrFile = name.c;
  ofn.nMaxFile = name.s;
  ofn.lpstrFileTitle=NULL; 
  ofn.nMaxFileTitle=0;
  ofn.lpstrInitialDir=NULL;
  ofn.lpstrTitle = title; //"Choose a file to open";
#ifndef OFN_FORCESHOWHIDDEN
#define OFN_FORCESHOWHIDDEN 0x10000000
#endif
  ofn.Flags=OFN_FORCESHOWHIDDEN;
  if (no_RO)    ofn.Flags |= OFN_HIDEREADONLY;
  if (!for_save) ofn.Flags |= OFN_FILEMUSTEXIST;
  ofn.nFileOffset=0;
  ofn.nFileExtension=0;
  ofn.lpstrDefExt=0; //"reg"
  ofn.lCustData=0;
  ofn.lpfnHook=NULL;
  ofn.lpTemplateName=NULL;
  if ((for_save? GetSaveFileName : GetOpenFileName)(&ofn)) {
    name.checklen();
    return 0;
  }
  DWORD err = CommDlgExtendedError();
  if (err) CommDlgErrMsgDlgBox(_T("DisplayOFNdlg"), err);
  return 1;
}

inline TCHAR *chomp(TCHAR *c) {
  if (!*c) return c;
  TCHAR *d = _tcschr(c, 0);
  if (d[-1] == '\n') {
    *--d = 0;
    if (d == c) return d;
  }
  if (d[-1] == '\r') *--d = 0;
  return d;
}

TCHAR cslashed(TCHAR c) {
  switch(c) {
  //case '\\': return '\\';
  //case '"': return '"';
  //case '\'': return '\'';
  case '0': return '\0';
  case 'n': return '\n';
  case 'r': return '\r';
  case 't': return '\t';
  default: return c;
  }
}

TCHAR *gethex(TCHAR *c, DWORD &d) {
  for(d = 0; isxdigit(*c); c++) 
    d = (d<<4)| ((*c<='9')? *c-'0' : toupper(*c)-'A'+10);
  return c;
}

int LoadDump8bit(const TCHAR *fname) {
  FILE *f = _tfopen(fname, _T("r"));
  FILE *dbgf = 0;//fopen("F:\\mmmm\\Misc\\regedt33\\dbgf.txt", "w");
  if (!f) {
    MessageBox(MainWindow, fname, _T("Could not read file"), MB_ICONERROR);
    return 1;
  }
  TCHAR buf[2048], buf_vn[2048];
  std::vector<unsigned char> buf_vd;
  bool chkid = true;
  bool bin_line_cont = false; DWORD type = 0;
  bool name_set = false;
  auto_close_hkey hk((HKEY)INVALID_HANDLE_VALUE);
  while(_fgetts(buf, 2048, f)) {
    TCHAR *e = chomp(buf);
    if (chkid) {
      if (_tcscmp(buf, _T("REGEDIT4"))) {
        MessageBox(MainWindow, fname, _T("Incorrect file signature"), MB_ICONERROR);
        return 2;
      }
      chkid = false;
      continue;
    }
    if (!*buf) {
      if (dbgf) _ftprintf(dbgf, _T("CloseKey_NHC\n"));
      CloseKey_NHC(hk.hk); hk.hk = (HKEY)INVALID_HANDLE_VALUE;
      continue;
    }
    if (*buf == '[') {
      if (e[-1] != ']') {
        MessageBox(MainWindow, fname, _T("Incorrect file format"), MB_ICONERROR);
        return 3;
      }
      *--e = 0;
      TCHAR *keyname = buf + 1;
      //TranslateKeyName(keyname, );
      CloseKey_NHC(hk.hk); if (dbgf) _ftprintf(dbgf, _T("CloseKey_NHC\n"));
      hk.hk = CreateKeyByName(keyname, 0, KEY_WRITE); if (dbgf) _ftprintf(dbgf, _T("CreateKeyByName(%s)\n"), keyname);
      continue;
    }
    if (*buf == '"' || *buf == '@') {
      TCHAR *c = buf, *v = buf_vn;
      if (*c != '@') for(c++; *c && *c != '"'; c++) {
        if (*c == '\\') *v++ = cslashed(*++c);
        else *v++ = *c;
      }
      if (!*c || c[1] != '=') continue;
      name_set = true;
      *v = 0;
      c += 2;
      buf_vd.clear();
      DWORD D = 0;
      bin_line_cont = false; type = 0;
      switch(*c) {
      case '"':
        for(c++; *c && *c != '"'; c++) {
          if (*c == '\\') buf_vd.push_back(cslashed(*++c));
          else buf_vd.push_back(*c);
        }
        buf_vd.push_back(0);
        type = 1;
        break;
      case 'h':
        c += 3; type = 3;
        if (*c == '(') {
          c++;
          c = gethex(c, type);
          if (*c != ')') break;
          c++;
        }
        do {
          c++;
          c = gethex(c, D);
          buf_vd.push_back(static_cast<unsigned char>(D));
        } while(*c == ',' && isxdigit(c[1]));
        if (*c == ',' && c[1] == '\\') bin_line_cont = true;
        break;
      case 'd':
        c += 6; buf_vd.resize(4);
        c = gethex(c, D);
        *(DWORD*)&buf_vd.front() = D;
        type = 4;
        break;
      }
      if (bin_line_cont) continue;
    }
    if (*buf == ' ' && bin_line_cont) {
      TCHAR *c = buf;
      while(*c == ' ' || *c == '\t') c++;
      bin_line_cont = false;
      if (!isxdigit((unsigned char)*c)) continue;
      do {
        c++; DWORD D;
        c = gethex(c, D);
        buf_vd.push_back(static_cast<unsigned char>(D));
      } while(*c == ',' && isxdigit(c[1]));
      if (*c == ',' && c[1] == '\\') { bin_line_cont = true; continue; }
    }
    if (name_set) {
      if (dbgf) _ftprintf(dbgf, _T("RegSetValueEx(%s, ..., sz = %zu)\n"), buf_vn, buf_vd.size());
      RegSetValueEx(hk.hk, buf_vn, NULL, type, (BYTE*)&buf_vd.front(), buf_vd.size());
      name_set = false;
    }
  }
  if (ferror(f)) {
    MessageBox(MainWindow, fname, _T("File read error"), MB_ICONERROR);
  }
  fclose(f);
  if (dbgf) fclose(dbgf);
  return 0;
}
