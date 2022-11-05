#include "regedit.h"
#include "resource.h"

int SearchReplaceForKey(const TCHAR *key, const TCHAR *subkey = 0);
int SearchReplaceSubkeys(const TCHAR *key, HKEY hkey);
extern HWND RplProgrDlg;

INT_PTR CALLBACK DialogSR (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK DialogSVT (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK DialogReplProgress (HWND,UINT,WPARAM,LPARAM);

INT_PTR CALLBACK EditBinary (HWND,UINT,WPARAM,LPARAM);
int MakeCEscapes(const TCHAR *c, int l, achar &out);
bool MakeCEscapes(achar &inout);

/*** Search & Replace settings ***/
TCHAR *replstart = 0, *replend = 0, *repl_logfname = 0;
achar repl_find, repl_with, repl_find_e, repl_with_e;
TCHAR *replvalstart = 0;
int replstart_l = 0, replend_l = 0;
bool repl_igncase = true, repl_fake = false, repl_pause = false;
bool repl_valnames = true, repl_valdata = true, repl_keynames = false;
bool srch_alphord = false;
bool srch_ceeditval = false, repl_ceeditval = false;
const bool srchval_types_init[12] = { 0, 1/*SZ*/, 1/*EXP_SZ*/, 0, 0, 0, 0/*SL*/, 1/*MSZ*/, 0, 0, 0, 0/*others*/ };
bool srchval_types[12] = { 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0 }; //same as above!!
/*** ---------------- settings ***/

int msgres = true;
FILE *repl_logfile = 0;
bool replace_all_vals = false, replace_dstvalexists_all = false;
bool replace_all_keys = false, replace_dstkeyexists_all = false;
bool repl_keys_moved = false;
fchar tts;
int cnt_fnd_in_key = 0, cnt_fnd_in_name = 0, cnt_fnd_in_data = 0;

int first_it_len(const TCHAR *c) {
    if (const TCHAR *e = _tcschr(c, '\\')) return e - c;
    else return _tcslen(c);
}

void DoSearchAndReplace(HWND hwnd, bool nodlg, bool onlyfind) {
  if (!nodlg) {
    repl_fake = onlyfind;
    extern TCHAR *currentitem;
    if (replstart) free(replstart);
    replstart = _tcsdup(currentitem);
  }
  if (nodlg || DialogBox(hInst,_T("SEARCHREPLACE"),hwnd,DialogSR)) {
    if (!*repl_find.c && !repl_fake ) {
      MessageBox(hwnd, _T("Can't replace empty string!"), _T("Failed"), MB_ICONSTOP);
      return;
    }
    if (!replstart) replstart = _tcsdup(_T(""));
    if (!replend) replend = _tcsdup(_T(""));
    if (*replstart && *replend && _tcsicmp(replstart, replend) > 0 && 
      _tcsnicmp(replstart, replend, _tcslen(replend))) {
      MessageBox(hwnd, _T("Empty range specified!"), _T("Regedit warning"), MB_ICONWARNING);
      return;
    }
    if (repl_logfname && *repl_logfname) {
      repl_logfile = _tfopen(repl_logfname, _T("w"));
      if (!repl_logfile) {
        MessageBox(hwnd, repl_logfname, _T("Could not write to file:"), MB_ICONSTOP);
        return;
      }
    }
    RplProgrDlg = CreateDialog(hInst,_T("REPLACEINPROGRESS"),hwnd,DialogReplProgress);
    ShowWindow(RplProgrDlg, SW_SHOW);
    msgres = true;
    replace_all_vals = false, replace_dstvalexists_all = false;
    replace_all_keys = false, replace_dstkeyexists_all = false;
    repl_keys_moved = false;

    const int rplsfl = first_it_len(replstart), rplefl = first_it_len(replend);
    const bool km = *replstart && rplsfl == rplefl && !_tcsnicmp(replstart, replend, rplsfl);
    TCHAR *s = !replstart[rplsfl]? _T("") : replstart + rplsfl + 1, 
          *e = !replend[rplefl]? _T("") : replend + rplefl + 1;

	TCHAR *s1 = s, *e1 = e, *const s2 = s, *const e2 = e, *fullcs;
    if (km) {
      for(; *s1 && _totlower(*s1) == _totlower(*e1); s1++, e1++) {
        if (*s1 == '\\') s = s1 + 1, e = e1 + 1;
      }
      if ((*s1 == '\\' && !*e1) || (*e1 == '\\' && !*s1) || (!*s1 && !*e1)) {
        s = *s1? s1 + 1 : s1;
        e = *e1? e1 + 1 : e1;
        fullcs = _tcsdup(*s1? e2 : s2);
        if (*s == '\\' && !s[1]) s++;
        if (*e == '\\' && !e[1]) e++;
      } else {
        if (s != s2) { s[-1] = 0; fullcs = _tcsdup(s2); s[-1] = '\\'; }
        else fullcs = 0;
      }
    } else {
      fullcs = 0;
    }
    
    cnt_fnd_in_key = cnt_fnd_in_name = cnt_fnd_in_data = 0;
    for(size_t n = 0; n < rkeys.size(); n++) {
      if (*replstart && _tcsnicmp(replstart, rkeys.v[n].name, _tcslen(rkeys.v[n].name)) > 0) continue;
      if (*replend && _tcsnicmp(replend, rkeys.v[n].name, _tcslen(rkeys.v[n].name)) < 0) continue;
      HKEY searchroot;
      if (RegOpenKeyEx(rkeys.v[n].ki.hkey, fullcs && *fullcs? fullcs : 0, 0, KEY_READ, &searchroot) != 0)
        continue; //m.b. some error message?
      RegCloseKey(searchroot);
      SearchReplaceForKey(rkeys.v[n].name, fullcs);
    }
    if (RplProgrDlg) {
      DestroyWindow(RplProgrDlg); RplProgrDlg = 0;
      TCHAR msg[200] = _T("Finished searching through registry\n\nFound nothing");
      if (cnt_fnd_in_key || cnt_fnd_in_name || cnt_fnd_in_data)
        _stprintf(msg + 37, _T("Found:\nIn key names %i times\nIn value names: %i times\nIn value data: %i times"),
                cnt_fnd_in_key, cnt_fnd_in_name, cnt_fnd_in_data);
      MessageBox(hwnd, msg, _T("Done!"),0);
    }
    if (repl_logfile) fclose(repl_logfile);
    if (repl_keys_moved) {
      SendMessage(hwnd,WM_COMMAND,340,0);
    }
    free(fullcs);
  }
}

void DoSearchAndReplaceNext(HWND hwnd) {
  if (RplProgrDlg) { SetFocus(RplProgrDlg); return; }
  if (replstart) free(replstart);
  if (replvalstart) free(replvalstart), replvalstart = 0;
  extern TCHAR *currentitem;
  replstart = _tcsdup(currentitem);
  extern HWND ListW;
  int n = ListView_GetNextItem(ListW, -1, LVNI_SELECTED);
  if (n >= 0) {
    fchar buf(malloc(2048));
    ListView_GetItemText(ListW, n, 0, buf.c, 2048);//!?
    if (buf.c && *buf.c) replvalstart = _tcsdup(buf.c);
  }
  DoSearchAndReplace(hwnd, repl_find.c != 0, false);
}

void TriggerReplace(HWND hwnd, bool is_search) {
  SetWindowText(hwnd, is_search? _T("Search") : _T("Search & Replace"));
  ShowWindow(GetDlgItem(hwnd, IDC_EDIT_REPLACE), is_search? SW_HIDE : SW_SHOW);
  ShowWindow(GetDlgItem(hwnd, IDC_STATIC_REPLACE), is_search? SW_HIDE : SW_SHOW);
  ShowWindow(GetDlgItem(hwnd, IDC_ALPHORD), !is_search? SW_HIDE : SW_SHOW);
  bool cval = SendDlgItemMessage(hwnd, IDC_C_SHOWSTRTYPE, BM_GETCHECK, 0,0) != 0;
  ShowWindow(GetDlgItem(hwnd, IDC_B_CALL_BE_RPL), cval && !is_search? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hwnd, IDC_C_CESC_RPL), cval && !is_search? SW_SHOW : SW_HIDE);
}

int srw_w = 0, srw_w1 = 0, srw_h = 0;
HWND dToolTip;
void CreateDtoolTip(HWND hwnd) {
  dToolTip = CreateWindowEx( 0, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, 20, 20, hwnd, NULL, hInst, NULL);
  SendMessage(dToolTip, TTM_SETMAXTIPWIDTH, 0, 300);
}

void InitTT(HWND hwnd, UINT msg) {
  TOOLINFO ti;
  ZeroMemory(&ti, sizeof(ti));
  ti.cbSize = sizeof(ti);
  ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  ti.hwnd = hwnd;
  ti.hinst = hInst;
  ti.lpszText = LPSTR_TEXTCALLBACK;
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_BUTTON_SELTYPE);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.lpszText = _T("Double click to insert current key name");
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_S_STARTKEY);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_S_ENDKEY);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.lpszText = _T("Double click to insert current value name");
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_S_STARTVAL);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.lpszText = _T("Show \"start after value\" edit box");
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_C_SHOWSTARTVAL);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.lpszText = _T("Show advanced settings for 'search' and 'replace' fields");
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_C_SHOWSTRTYPE);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.lpszText = _T("Browse");
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_BUTTON_BROWSE);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.lpszText = _T("Edit as binary data");
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_B_CALL_BE_SRCH);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_B_CALL_BE_RPL);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.lpszText = _T("We have C/C++-style escape sequences\nbeginning with backslash ('\\') character");
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_C_CESC_SRCH);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
  ti.uId = (UINT_PTR)GetDlgItem(hwnd, IDC_C_CESC_RPL);
  SendMessage(dToolTip, msg, 0, (LPARAM)&ti);
}

void CheckSelType(HWND hwnd, bool cf = false) {
  bool t = !!memcmp(srchval_types_init, srchval_types, sizeof(srchval_types));
  SetDlgItemText(hwnd, IDC_BUTTON_SELTYPE, t? _T("!!!") : _T("..."));
  if (!cf) return;
  int n;
  for(n = 0; n < 12; n++) if (srchval_types[n]) break;
  SendDlgItemMessage(hwnd, IDC_VALDATA, BM_SETCHECK,  n < 12, 0);
  EnableWindow(GetDlgItem(hwnd, IDC_VALDATA), n < 12);
}

INT_PTR CALLBACK DialogSR (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    bool prevval, cval; int k;
    switch (msg) {
	case WM_INITDIALOG: 
      SendDlgItemMessage(hwnd, IDC_CASE,     BM_SETCHECK, repl_igncase, 0);
      SendDlgItemMessage(hwnd, IDC_NOCHNG,   BM_SETCHECK, repl_fake, 0);
      SendDlgItemMessage(hwnd, IDC_VALNAMES, BM_SETCHECK, repl_valnames, 0);
      SendDlgItemMessage(hwnd, IDC_VALDATA,  BM_SETCHECK, repl_valdata, 0);
      SendDlgItemMessage(hwnd, IDC_KEYNAMES, BM_SETCHECK, repl_keynames, 0);
      SendDlgItemMessage(hwnd, IDC_ALPHORD,  BM_SETCHECK, srch_alphord, 0);
      SendDlgItemMessage(hwnd, IDC_C_CESC_SRCH, BM_SETCHECK, srch_ceeditval, 0);
      SendDlgItemMessage(hwnd, IDC_C_CESC_RPL,  BM_SETCHECK, repl_ceeditval, 0);
      SetEditCtrlHistAndText(hwnd, IDC_STARTKEY, replstart);
      SetEditCtrlHistAndText(hwnd, IDC_ENDKEY, replend);
      if (srch_ceeditval) MakeCEscapes(repl_find_e);
      SetEditCtrlHistAndText(hwnd, IDC_EDIT_SERACH, repl_find_e.c);
      if (repl_ceeditval) MakeCEscapes(repl_with_e);
      SetEditCtrlHistAndText(hwnd, IDC_EDIT_REPLACE, repl_with_e.c);
      SetEditCtrlHistAndText(hwnd, IDC_EDIT_LOGFNAME, repl_logfname);
      if (repl_fake) TriggerReplace(hwnd, true);
      CreateDtoolTip(hwnd);
      InitTT(hwnd, TTM_ADDTOOL);
      CheckSelType(hwnd);
      {
        RECT rc, rc1;
        GetWindowRect(GetDlgItem(hwnd, IDC_EDIT_SERACH), &rc);
        GetWindowRect(GetDlgItem(hwnd, IDC_B_CALL_BE_SRCH), &rc1);
        srw_w = rc.right - rc.left;
        srw_w1 = rc1.left - rc.left - 4;
        srw_h = rc.bottom - rc.top;
      }
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
	  switch (LOWORD (wParam)) {
	  case IDOK: 
        srch_ceeditval = SendDlgItemMessage(hwnd, IDC_C_CESC_SRCH, BM_GETCHECK, 0,0) != 0;
        repl_ceeditval = SendDlgItemMessage(hwnd, IDC_C_CESC_RPL,  BM_GETCHECK, 0,0) != 0;
        getDlgItemText(replstart, hwnd, IDC_STARTKEY);
        getDlgItemText(replend, hwnd, IDC_ENDKEY);
        getDlgItemText(replvalstart, hwnd, IDC_EDIT_STARTVAL);
        if (srch_ceeditval) repl_find_e.GetDlgItemTextUnCEsc(hwnd, IDC_EDIT_SERACH);
        else repl_find_e.GetDlgItemText(hwnd, IDC_EDIT_SERACH);
        if (repl_ceeditval) repl_with_e.GetDlgItemTextUnCEsc(hwnd, IDC_EDIT_REPLACE);
        else repl_with_e.GetDlgItemText(hwnd, IDC_EDIT_REPLACE);
        getDlgItemText(repl_logfname, hwnd, IDC_EDIT_LOGFNAME);
        repl_igncase  = SendDlgItemMessage(hwnd, IDC_CASE,     BM_GETCHECK, 0,0) != 0;
        repl_fake     = SendDlgItemMessage(hwnd, IDC_NOCHNG,   BM_GETCHECK, 0,0) != 0;
        repl_valnames = SendDlgItemMessage(hwnd, IDC_VALNAMES, BM_GETCHECK, 0,0) != 0;
        repl_valdata  = SendDlgItemMessage(hwnd, IDC_VALDATA,  BM_GETCHECK, 0,0) != 0;
        repl_keynames = SendDlgItemMessage(hwnd, IDC_KEYNAMES, BM_GETCHECK, 0,0) != 0;
        srch_alphord  = SendDlgItemMessage(hwnd, IDC_ALPHORD, BM_GETCHECK, 0,0) != 0;
        repl_find = repl_find_e; repl_with = repl_with_e;
        if (repl_igncase) repl_find.strlwr();
        replstart_l = _tcslen(replstart), replend_l = _tcslen(replend);
        if (replvalstart && !*replvalstart) free(replvalstart), replvalstart = 0;
        EndDialog(hwnd,1);
        break;
	  case IDCANCEL: EndDialog(hwnd,0); RplProgrDlg = 0; break; 
      case IDC_NOCHNG:
        prevval = repl_fake;
        repl_fake = SendDlgItemMessage(hwnd, IDC_NOCHNG, BM_GETCHECK, 0,0) != 0;
        if (prevval != repl_fake) TriggerReplace(hwnd, repl_fake);
        break;
      case IDC_C_SHOWSTARTVAL:
        cval = SendDlgItemMessage(hwnd, IDC_C_SHOWSTARTVAL, BM_GETCHECK, 0,0) != 0;
        ShowWindow(GetDlgItem(hwnd, IDC_S_STARTVAL), cval? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(hwnd, IDC_EDIT_STARTVAL), cval? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(hwnd, IDOK), cval? SW_HIDE : SW_SHOW); //;-)
        ShowWindow(GetDlgItem(hwnd, IDCANCEL), cval? SW_HIDE : SW_SHOW);
        break;
      case IDC_C_SHOWSTRTYPE:
        cval = SendDlgItemMessage(hwnd, IDC_C_SHOWSTRTYPE, BM_GETCHECK, 0,0) != 0;
        ShowWindow(GetDlgItem(hwnd, IDC_B_CALL_BE_RPL), cval && !repl_fake? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(hwnd, IDC_C_CESC_RPL), cval && !repl_fake? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(hwnd, IDC_B_CALL_BE_SRCH), cval? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(hwnd, IDC_C_CESC_SRCH), cval? SW_SHOW : SW_HIDE);
        SetWindowPos(GetDlgItem(hwnd, IDC_EDIT_SERACH), 0,  0, 0, cval? srw_w1 : srw_w, srw_h,  SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(hwnd, IDC_EDIT_REPLACE), 0,  0, 0, cval? srw_w1 : srw_w, srw_h,  SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        break;
      case IDC_VALNAMES: case IDC_VALDATA: case IDC_KEYNAMES:
        cval = SendDlgItemMessage(hwnd, IDC_VALNAMES, BM_GETCHECK, 0,0) != 0
            || SendDlgItemMessage(hwnd, IDC_VALDATA,  BM_GETCHECK, 0,0) != 0
            || SendDlgItemMessage(hwnd, IDC_KEYNAMES, BM_GETCHECK, 0,0) != 0;
        EnableWindow(GetDlgItem(hwnd, IDOK), cval);
        break;
      case IDC_S_STARTKEY: case IDC_S_ENDKEY:
        extern TCHAR *currentitem;
        if (HIWORD(wParam) == 1 && currentitem) {
          SetDlgItemText(hwnd, LOWORD(wParam) == IDC_S_STARTKEY? IDC_STARTKEY : IDC_ENDKEY, currentitem);
        }
        break;
      case IDC_S_STARTVAL: 
        {
          extern HWND ListW;
          int n = ListView_GetNextItem(ListW, -1, LVNI_SELECTED);
          if (n < 0) break;
          fchar buf(malloc(2048));
          ListView_GetItemText(ListW, n, 0, buf.c, 2048);//!?
          SetDlgItemText(hwnd, IDC_EDIT_STARTVAL, buf.c);
        }
        break;
      case IDC_BUTTON_BROWSE: {
        achar tname;
        tname.GetDlgItemText(hwnd, IDC_EDIT_LOGFNAME);
        if (!DisplayOFNdlg(tname, _T("Choose a file"), _T("log files\0*.log\0all files\0*.*\0"), true, true)) {
          SetDlgItemText(hwnd, IDC_EDIT_LOGFNAME, tname.c? tname.c : _T(""));
        }
        break;}
      case IDC_BUTTON_SELTYPE:
        k = DialogBox(hInst, _T("SRCHVALTYPE"), hwnd, DialogSVT);
        if (k) {
          CheckSelType(hwnd, true);
          SendMessage(hwnd, WM_COMMAND, IDC_VALDATA, 0);
        }
        break;
      case IDC_B_CALL_BE_SRCH: case IDC_B_CALL_BE_RPL: 
        {
          val_ed_dialog_data ed; ed.flag1 = 0, ed.readonly = 0;
          bool iss = LOWORD(wParam) == IDC_B_CALL_BE_SRCH;
          bool is_esc = SendDlgItemMessage(hwnd, iss? IDC_C_CESC_SRCH : IDC_C_CESC_RPL, BM_GETCHECK, 0, 0) != 0;
          ed.name.c = _tcsdup(iss? _T("Search:") : _T("Replace:"));// :-(
          if (!is_esc) ed.data.GetDlgItemText(hwnd, iss? IDC_EDIT_SERACH : IDC_EDIT_REPLACE);
          else ed.data.GetDlgItemTextUnCEsc(hwnd, iss? IDC_EDIT_SERACH : IDC_EDIT_REPLACE);
          bool ok = DialogBoxParam(hInst, _T("EDBINARY_AV"), hwnd, EditBinary, (LPARAM)&ed) >=0 && ed.newdata.c;
          if (ok) {
            MakeCEscapes(ed.newdata.c, ed.newdata.l, ed.data);
            SetDlgItemText(hwnd, iss? IDC_EDIT_SERACH : IDC_EDIT_REPLACE, ed.data.c);
            if (ed.newdata.l != ed.data.l)
              SendDlgItemMessage(hwnd, iss? IDC_C_CESC_SRCH : IDC_C_CESC_RPL, BM_SETCHECK, true, 0);
          }
        }
      }
	  return 1;
    
    case WM_NOTIFY:
      switch (((LPNMHDR)lParam)->code) {
      case TTN_NEEDTEXT:
        if (((LPNMHDR)lParam)->idFrom == (UINT_PTR)GetDlgItem(hwnd, IDC_BUTTON_SELTYPE)) {
          int n, l = 0;
          for(n = l = 0; n < 12; n++) if (srchval_types[n]) {
            l += n != 11? _tcslen(TypeCName(n)) + 1 : 7;
          }
          TCHAR *c = tts.c = (TCHAR*)realloc(tts.c, (l + 1) * sizeof(TCHAR));
          for(n = l = 0; n < 12; n++) if (srchval_types[n]) {
            const TCHAR *t =  n != 11? TypeCName(n) : _T("others");
            _tcscpy(c, t);
            c += _tcslen(t);
            *c++ = '\n';
          }
          if (c > tts.c) 
            *--c = 0, ((LPTOOLTIPTEXT)lParam)->lpszText = tts.c;
          else ((LPTOOLTIPTEXT)lParam)->lpszText = _T("(empty)");
          return 1;
        }
        break;
      }
      break;
	}
	return 0;
}

INT_PTR CALLBACK DialogSVT (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  int n;
  switch (msg) {
  case WM_INITDIALOG: 
    for(n = 0; n < 12; n++) 
      SendDlgItemMessage(hwnd, IDC_C_0 + n /*!!!*/, BM_SETCHECK, srchval_types[n], 0);
    break;
  case WM_CLOSE: EndDialog(hwnd, 0); return 1;
  case WM_COMMAND:
    switch (LOWORD (wParam)) {
	  case IDCANCEL: EndDialog(hwnd, 0); break; 
	  case IDOK: 
        for(n = 0; n < 12; n++) 
          srchval_types[n] = SendDlgItemMessage(hwnd, IDC_C_0 + n /*!!!*/, BM_GETCHECK, 0, 0) != 0;
        EndDialog(hwnd, 1);
        break;
    }
  }
  return 0;
}

INT_PTR CALLBACK DialogAR (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    confirm_replace_dialog_data *crd;
    switch (msg) {
	case WM_INITDIALOG: 
      crd = (confirm_replace_dialog_data*)lParam;
      if (crd->keyname) SetDlgItemText(hwnd, IDC_KEYNAME, crd->keyname);
      if (crd->oldvalue) SetDlgItemText(hwnd, IDC_OLDNAME, crd->oldvalue);
      if (crd->newvalue) SetDlgItemText(hwnd, IDC_NEWNAME, crd->newvalue);
      else EnableWindow(GetDlgItem(hwnd, IDC_NEWNAME), 0),
           EnableWindow(GetDlgItem(hwnd, IDC_ST_NEWNAME), 0);
      if (crd->olddata) {
        achar t; MakeCEscapes(crd->olddata, crd->olddatalen, t);
        SetDlgItemText(hwnd, IDC_OLDDATA, t.c);
      }
      if (crd->newdata) {
        achar t; MakeCEscapes(crd->newdata, crd->newdatalen, t);
        SetDlgItemText(hwnd, IDC_NEWDATA, t.c);
      }
      else EnableWindow(GetDlgItem(hwnd, IDC_NEWDATA), 0),
           EnableWindow(GetDlgItem(hwnd, IDC_ST_NEWDATA), 0);
      if (crd->def_next) {
        SendMessage(hwnd, DM_SETDEFID, IDC_B_SKIP, 0);
        SetFocus(GetDlgItem(hwnd, IDC_B_SKIP));
        return 0; // for SetFocus
      }
      return 1;
    case WM_CLOSE: EndDialog(hwnd,2); return 1;
    case WM_COMMAND:
	  switch (LOWORD (wParam)) {
	  case IDCANCEL: EndDialog(hwnd,0); break; 
	  case IDOK: EndDialog(hwnd,1); break;
	  case IDC_B_SKIP: EndDialog(hwnd,2); break;
	  case IDC_B_ALL: EndDialog(hwnd,3); break;
      }
	  return 1;
	}
	return 0;
}

INT_PTR CALLBACK DialogReplProgress (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    switch (msg) {
	case WM_INITDIALOG: repl_pause = 0; 
      if (repl_fake) SetWindowText(hwnd, _T("Searching..."));
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); RplProgrDlg = 0; return 1;
    case WM_COMMAND:
	  switch (LOWORD (wParam)) {
      case IDPAUSE: repl_pause = !repl_pause; SetDlgItemText(hwnd, IDPAUSE, repl_pause? _T("&Resume") : _T("&Pause")); break; 
	  case IDCANCEL: EndDialog(hwnd,0); RplProgrDlg = 0; break; 
      }
	  return 1;
    //case WM_KEYDOWN:
      //Beep(300,10);
      //break;
    case WM_CHAR:
      if ((TCHAR)wParam == 27) SendMessage(hwnd, WM_COMMAND, IDPAUSE, 0); else
      if ((TCHAR)wParam == 'S' || (TCHAR)wParam == 's') SendMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
      //Beep(300,10);
      return 1;
	}
	return 0;
}

int do_sr_str(const TCHAR *oldstr, int oldlen, TCHAR *&newstr) {
  int const repl_find_len = repl_find.l / sizeof(TCHAR);
  int const repl_with_len = repl_with.l / sizeof(TCHAR);
  int const lendiff = repl_find_len - repl_with_len; //note! repl_find_l != 0
  int newasz = oldlen + 10 * (lendiff > 0? lendiff : 0), newsz = 0;
  TCHAR *news = (TCHAR*)malloc(newasz * sizeof(TCHAR));
  TCHAR rf0 = *repl_find.c;
  int n;
  for (n = 0; n <= oldlen - repl_find_len;) {
    if (newasz - newsz < repl_with_len*2 + 10)
	  news = (TCHAR*)realloc(news, (newasz = newsz + repl_with_len*2 + 10) * sizeof(TCHAR));
    if ((repl_igncase? _totlower(oldstr[n]) : oldstr[n]) == rf0) {
      int k;
      if (repl_igncase) for(k = 0; k < repl_find_len && _totlower(oldstr[n + k])==repl_find.c[k]; k++);
      else for(k = 0; k < repl_find_len && oldstr[n + k]==repl_find.c[k]; k++);
      if (k == repl_find_len) {
        memcpy(news + newsz, repl_with.c, repl_with_len * sizeof(TCHAR));
        n += k, newsz += repl_with_len;
        continue;
      }
    }
    news[newsz++] = oldstr[n++];
  }
  if (newasz - newsz < oldlen - n) news = (TCHAR*)realloc(news, (newasz = newsz + oldlen - n) * sizeof(TCHAR));
  for(; n < oldlen; n++) news[newsz++] = oldstr[n];
  newstr = news;
  return newsz;
}

struct rec_srk {
  TCHAR *name, *data;
  int data_len;
  bool found_in_name, found_in_data;
};

inline long RegEnumValue(HKEY hk, DWORD idx, achar &name, DWORD *type, achar &data) {
    name.l = name.s / sizeof(TCHAR), data.l = data.s;
    long rv = RegEnumValue(hk, idx, name.c, &name.l, NULL, type, (BYTE*)data.c, &data.l);
	name.l *= sizeof(TCHAR);
	if (rv && (name.l >= name.s || data.l >= data.s)) {
      name.resize();
      data.resize();
      rv = RegEnumValue(hk, idx, name.c, &name.l, NULL, type, (BYTE*)data.c, &data.l);
	  name.l *= sizeof(TCHAR);
    }
    return rv;
}

bool is_substr(const achar &where, const achar &what) {
  if (what.l > where.l) return false;
  const TCHAR *t = what.c; int l = what.l;
  TCHAR cc = *t;
  const TCHAR *c = where.c, *e = c + where.l / sizeof(TCHAR) - l / sizeof(TCHAR) + (l != 0);
#ifdef _UNICODE
  while(c = wmemchr(c, cc, e - c)) {
#else
  while (c = (char *)memchr(c, cc, e - c)) {
#endif
	if (!memcmp(c, t, l)) return true;
    c++;
    if (c >= e) return false;
  }
  return false;
}

TCHAR *mdup(const achar &src) {
  TCHAR *r = (TCHAR*)malloc(src.l + sizeof(TCHAR));
  memcpy(r, src.c, src.l);
  r[src.l / sizeof(TCHAR)] = 0;
  return r;
}

bool SearchReplaceKeyValues(HKEY hkey, int ro, const TCHAR *keyname, const TCHAR *rpvs) {
  if (!repl_valnames && !repl_valdata) return 0;
  value_iterator i(hkey);
  if (i.err()) return 0;
  achar vnc, vdc;
  bool found = false;
  rec_srk *srk = 0; int srksize = 0;
  for(; !i.end(); i++) {
    //int rv = RegEnumValue(hkey, n, vn, &type, vd);
    if (!i.is_ok) continue;
    if (rpvs && *rpvs && _tcsicmp(i.name.c, rpvs) <= 0) continue;
    if (repl_igncase) { vnc = i.name; vnc.strlwr(); }
    bool found_in_name = repl_valnames && is_substr(repl_igncase? vnc : i.name, repl_find);
    bool found_in_data = false;
    if (repl_valdata && (i.type < 11 && srchval_types[i.type] || i.type >= 11 && srchval_types[11])) {
      if (repl_igncase) { vdc = i.data; vdc.strlwr(); }
      found_in_data = is_substr(repl_igncase? vdc : i.data, repl_find);
    }
    if (found_in_name || found_in_data) {
      found = true;
      if (ro != 2) cnt_fnd_in_name += found_in_name, cnt_fnd_in_data += found_in_data;
      if (ro == 1 && repl_logfile) _ftprintf(repl_logfile, _T("%s\t%s\t%i\t%i\n"), keyname, i.name.c, found_in_name, found_in_data);
      if (!ro || ro == 1) { //udly code. not sure if I can use stl vector here
        srk = (rec_srk*) realloc(srk, ++srksize * sizeof(rec_srk));
        srk[srksize-1].name = _tcsdup(i.name.c);
        srk[srksize-1].data = ro == 1? mdup(i.data) : 0;
        srk[srksize-1].data_len = ro == 1? i.data.l : 0;
        srk[srksize-1].found_in_name = found_in_name;
        srk[srksize-1].found_in_data = found_in_data;
      }
    }
  }
  if (ro == 1) {
    for(int n = 0; n < srksize; free(srk[n].name), free(srk[n].data), n++) {
      if (!RplProgrDlg)
        continue;
      confirm_replace_dialog_data crd = { keyname, srk[n].name, 0
        , srk[n].data, srk[n].data_len, 0, 0, false};
      achar newdata;
      int l = DialogBoxParam(hInst,_T("ASKGOTOFOUND"),MainWindow, DialogAR, (LPARAM)&crd);
      while (l == 1) {
		val_ed_dialog_data dp;
        dp.keyname = keyname;
        dp.name.c = srk[n].name;
        if (!dp.EditValue(MainWindow)) {
          newdata.swap(dp.newdata);
          crd.newdata = newdata.c;
          crd.newdatalen = newdata.l;
          crd.def_next = true;
        }
        dp.name.c = 0;
        l = DialogBoxParam(hInst,_T("ASKGOTOFOUND"),MainWindow, DialogAR, (LPARAM)&crd);
      }
      if (l == 0 || l == 2 || l == 3) {
        if (l == 0 || l == 3) { if (RplProgrDlg) SendMessage(RplProgrDlg, WM_CLOSE, 0, 0); }
        if (l == 3) {
          extern HWND TreeW, ListW, LastFocusedW;
          ShowItemByKeyName(TreeW, keyname);
          SelectItemByValueName(ListW, srk[n].name);
          SetFocus(LastFocusedW = ListW);
        }
        continue;
      }
    }
  }
  if (ro)
    return found;
  TCHAR ffc = *repl_find.c;
  achar prevdata, prevvaldatprn;
  
  for(int n = 0; n < srksize; free(srk[n].name), n++) {
    if (!RplProgrDlg)
      continue;
    TCHAR *oldname = srk[n].name;
    fchar newname, newdata;

    DWORD type, newdatal;
    i.data.l = i.data.s;

    if (RegQueryValueEx(hkey, oldname, NULL, &type, i.data.b, &i.data.l) != ERROR_SUCCESS)
      continue;
    newdatal = i.data.l;
    if (srk[n].found_in_name) do_sr_str(oldname, _tcslen(oldname) + 1, newname.c);
    if (srk[n].found_in_data) newdatal = do_sr_str((TCHAR *)i.data.c, i.data.l / sizeof(TCHAR), newdata.c) * sizeof(TCHAR);//???

    bool is_rename = newname.c && _tcsicmp(oldname, newname.c);
    if (!replace_all_vals) {
      confirm_replace_dialog_data crd = { keyname, oldname, is_rename? newname.c : 0
        , i.data.c, i.data.l, newdata.c, newdatal, false};
      int l = DialogBoxParam(hInst,_T("ASKREPLACE"),MainWindow,DialogAR, (LPARAM)&crd);
      if (l == 3) replace_all_vals = true;
      else if (l == 0 || l == 2) {
        if (l == 0) { if (RplProgrDlg) SendMessage(RplProgrDlg, WM_CLOSE, 0, 0); break; }
        continue;
      }
    }
    if (is_rename && 
      RegQueryValueEx(hkey, newname.c, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
      DWORD prevtype = REG_SZ;
      if (repl_logfile || !replace_dstvalexists_all) {
        if (!prevdata.c) {
          prevdata.resize(max(i.data.s, (DWORD)31));
          prevvaldatprn.resize(prevdata.s * 3);
        }
        prevdata.l = prevdata.s;
        if (RegQueryValueEx(hkey, newname.c, NULL, &prevtype, prevdata.b, &prevdata.l) != ERROR_SUCCESS)
          _tcscpy(prevdata.c, _T("(Failed to retrieve value)"));
        GetValueDataString((char*)prevdata.c, prevvaldatprn.c, prevdata.l, prevtype);
      }
      if (repl_logfile)
        _ftprintf(repl_logfile, _T("Replace-target-exists: %s\t%s\t%s\t%s\t%i\t%i\n"), keyname, oldname, newname.c, prevvaldatprn.c, srk[n].found_in_name, srk[n].found_in_data);
      if (!replace_dstvalexists_all) {
        confirm_replace_dialog_data crd = { keyname, oldname, newname.c, 0, 0, prevvaldatprn.c, _tcslen(prevvaldatprn.c), false };
        int l = DialogBoxParam(hInst,_T("ASKDSTVALEXISTS"),MainWindow,DialogAR, (LPARAM)&crd);
        if (l == 3) replace_dstvalexists_all = true;
        else if (l == 0 || l == 2) {
          if (l == 0) { if (RplProgrDlg) SendMessage(RplProgrDlg, WM_CLOSE, 0, 0); break; }
          continue; //target exists!
        }
      }
    }

    if (RegSetValueEx(hkey,srk[n].found_in_name? newname.c : oldname, NULL, type, (BYTE*)(srk[n].found_in_data? newdata.c : i.data.c), newdatal) != ERROR_SUCCESS) {
      ErrMsgDlgBox(_T("ku-ku!\n"));
      continue;
    }
    if (repl_logfile) {
      if (is_rename)
        _ftprintf(repl_logfile, _T("Replace+name: %s\t%s\t%s\t%i\t%i\n"), keyname, oldname, newname.c, srk[n].found_in_name, srk[n].found_in_data);
      else
        _ftprintf(repl_logfile, _T("Replace: %s\t%s\t%i\t%i\n"), keyname, oldname, srk[n].found_in_name, srk[n].found_in_data);
    }
    if (is_rename) RegDeleteValue(hkey,oldname);

  }
  free(srk);
  return found;
}

bool very_loud = false;
int SearchReplaceForKey(const TCHAR *key, const TCHAR *subkey) {
  TCHAR *c;
  bool found = false;
  if (!msgres) return 2;
  if (subkey && *subkey) {
    c = (TCHAR*)alloca((_tcslen(key) + _tcslen(subkey) + 2) * sizeof(TCHAR));
    _stprintf(c, _T("%s\\%s"), key, subkey);
    key = c;
  } else if (repl_igncase) {
    c = (TCHAR*)alloca((_tcslen(key) + 1) * sizeof(TCHAR));
    _tcscpy(c, key);
    key = c;
  }
  DWORD keylen = _tcslen(key);
  if (_tcsnicmp(key, replstart, min<int>(keylen, _tcslen(replstart))) < 0 || _tcsnicmp(key, replend, min<int>(keylen, _tcslen(replend))) > 0) return 1;
  HKEY hkey = GetKeyByName(key, KEY_READ);
  if (hkey == INVALID_HANDLE_VALUE) return 3;
  SearchReplaceSubkeys(key, hkey);
  if (repl_logfile && very_loud) _ftprintf(repl_logfile, _T("\n[%s]\n"), key);
  const TCHAR *rpvs = replvalstart && !_tcsicmp(key, replstart)? replvalstart : 0;
  if ((found = SearchReplaceKeyValues(hkey, 2 - repl_fake, key, rpvs)) && !repl_fake) {
    HKEY hkeyw = 0;
    if (RegOpenKeyEx(hkey, 0, 0, KEY_READ | KEY_WRITE, &hkeyw) == 0) {
      SearchReplaceKeyValues(hkeyw, false, key, rpvs);
      RegCloseKey(hkeyw);
    } else {
      //log error
    }
  }
  RegCloseKey(hkey);
  if (rpvs) free(replvalstart), replvalstart = 0;
  if (repl_igncase) _tcslwr((TCHAR*)key);
  c = (TCHAR*)_tcsrchr(key, '\\');
  if (c && repl_keynames && _tcsstr(c + 1, repl_find.c)) {
    found = true;
    cnt_fnd_in_key++;
    fchar newc, newname;
    do_sr_str(c + 1, _tcslen(c + 1) + 1, newc.c);
    newname.c = (TCHAR*)malloc((_tcslen(newc.c) + (c - key) + 2) * sizeof(TCHAR));
    memcpy(newname.c, key, ((c - key) + 1) * sizeof(TCHAR));
    _tcscpy(newname.c + (c - key) + 1, newc.c);
    if (!replace_all_keys) {
      confirm_replace_dialog_data crd = { key, 0, newname.c, 0, 0, 0, 0, false };
      int l = DialogBoxParam(hInst, repl_fake? _T("ASKGOTOFOUNDKEY") : _T("ASKREPLACEKEY"), MainWindow, DialogAR, (LPARAM)&crd);
      if (repl_fake && l == 3) {
        l = 0;
        extern HWND TreeW;
        ShowItemByKeyName(TreeW, key);
      }
      if (l == 0 || l == 2) {
        if (l == 0) if (RplProgrDlg) SendMessage(RplProgrDlg, WM_CLOSE, 0, 0);
      } else {
        if (l == 3) replace_all_keys = true;
        //ops...
        MoveKey(key, newname.c);
        repl_keys_moved = true;
      }
    }
  }
  if (found) SetDlgItemText(RplProgrDlg, IDC_KEYNAME, key);
  //if (repl_logfile && found) _ftprintf(repl_logfile, _T("+\n"));
  do {
    MSG msg;
    while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
      if (msg.message == WM_QUIT) { 
        msgres = false; //repost. This is an emergency case.
        PostQuitMessage(msg.wParam);
        break; 
      }
      TranslateMessage (&msg);
      if (RplProgrDlg && msg.message == WM_CHAR) {
        //Beep(300,10);
        SendMessage(RplProgrDlg, WM_CHAR, msg.wParam, msg.lParam);
      }
      DispatchMessage(&msg);
    }
    if (repl_pause) { Sleep(50); continue; }
  } while(repl_pause && msgres);
  return 0;
}
int SearchReplaceSubkeys(const TCHAR *key, HKEY hkey) {
  int n;
  if (!RplProgrDlg || !msgres) return 1;
  DWORD sns = MAX_PATH + 1;
  TCHAR *sn = (TCHAR*)malloc(sns * sizeof(TCHAR));
  FILETIME ft;
  for(n = 0;; n++) {
    sns = MAX_PATH + 1;
    LONG rv = RegEnumKeyEx(hkey, n, sn, &sns, 0, 0,0, &ft);
    if (rv != 0) break;
    SearchReplaceForKey(key, sn);
    if (!RplProgrDlg) break;
  }
  free(sn);
  return 0;
}
