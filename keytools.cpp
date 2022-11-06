#include "regedit.h"
#include "resource.h"

rk_init_s rkeys;

HKEY rk_init_s::KeyByName(const TCHAR *name, const TCHAR **out) {
  const TCHAR *c = std::find(name, _tcschr(name, 0), '\\');
  achar part((c - name) * sizeof(TCHAR));
  *std::copy(name, c, part.c) = 0;
  n2kmap::iterator i = n2k.find(part.c);
  if (out) *out = *c? c + 1 : c;
  if (i == n2k.end()) return (HKEY)INVALID_HANDLE_VALUE;
  return i->second.hkey;
}

static HKRootName2Handle_item_type rkeys_basic[] = {
  {_T("HKCR"), {HKEY_CLASSES_ROOT,    _T("HKEY_CLASSES_ROOT"),    4, 0}},
  {_T("HKCU"), {HKEY_CURRENT_USER,    _T("HKEY_CURRENT_USER"),    4, 0}},
  {_T("HKLM"), {HKEY_LOCAL_MACHINE,   _T("HKEY_LOCAL_MACHINE"),   5, 0}},
  {_T("HKUS"), {HKEY_USERS,           _T("HKEY_USERS"),           5, 0}},
#ifndef _WIN32_WCE
  {_T("HKCC"), {HKEY_CURRENT_CONFIG,  _T("HKEY_CURRENT_CONFIG"),  4, 0}},
  {_T("HKDD"), {HKEY_DYN_DATA,        _T("HKEY_DYN_DATA"),        4, 0}},
  {_T("HKPD"), {HKEY_PERFORMANCE_DATA,_T("HKEY_PERFORMANCE_DATA"),4, 0}},
#endif
};

int rk_init_s::add(const TCHAR *name, HKEY hkey, int flags, HTREEITEM item) {
  HKRootName2Handle_item_type r = { name, {hkey, 0, flags, item}};
  if (!n2k.insert(n2kmap::value_type(name, r.ki)).second) return 1;
  k2n.insert(k2nmap::value_type(hkey, name));
  v.push_back(r);
  return 0;
}
rk_init_s::rk_init_s() {
  for(int n = 0; n < _countof(rkeys_basic); n++) {
    n2k.insert(n2kmap::value_type(rkeys_basic[n].name, rkeys_basic[n].ki));
    n2k.insert(n2kmap::value_type(rkeys_basic[n].ki.full_name, rkeys_basic[n].ki));
    k2n.insert(k2nmap::value_type(rkeys_basic[n].ki.hkey, rkeys_basic[n].name));
    lk2sk.insert(namap::value_type(rkeys_basic[n].ki.full_name, rkeys_basic[n].name));
    v.push_back(rkeys_basic[n]);
  }
}
int rk_init_s::remove(const TCHAR *name, HKRootName2Handle_item_type &out) {
  n2kmap::iterator i = n2k.find(name);
  if (i == n2k.end()) return 1;
  if (i->second.flags & CANT_REMOVE_KEY) return 2;
  const TCHAR *c = i->first;
  size_t n, k;
  for(n = 0, k = 0; n < v.size(); n++) {
    if (n != k) v[k] = v[n];
    if (v[n].name == c) out = v[n];
    else k++;
  }
  if (n == k) return 3; // something very strange...
  v.pop_back();
  n2k.erase(i);
  k2n.erase(out.ki.hkey);
  return 0;
}

const TCHAR *rk_init_s::ShortName2LongName(const TCHAR *name, const TCHAR **out) {
	TCHAR *c = (TCHAR*)_tcschr(name, '\\');
  if (c) *c = 0;
  n2kmap::iterator i = n2k.find((TCHAR*)name);
  if (c) *c = '\\';
  out && (*out = c? c + 1: _tcschr(name, 0));
  if (i == n2k.end()) return 0;
  return i->second.full_name;
}
const TCHAR *rk_init_s::LongName2ShortName(const TCHAR *name, const TCHAR **out) {
  TCHAR *c = (TCHAR*)_tcschr(name, '\\');
  if (c) *c = 0;
  namap::iterator i = lk2sk.find((TCHAR*)name);
  if (c) *c = '\\';
  out && (*out = c? c + 1: _tcschr(name, 0));
  if (i == lk2sk.end()) return 0;
  return i->second;
}

//Caller must free handle:
LONG lastRegErr = 0;
HKEY GetKeyByName(const TCHAR *c,REGSAM samDesired) {
  const TCHAR *c1;
  HKEY at = rkeys.KeyByName(c, &c1), rk;
  if (*c1 && at != (HKEY)INVALID_HANDLE_VALUE) {
	if (RegOpenKeyEx(at,c1,0,samDesired,&rk)==ERROR_SUCCESS) return rk;
	else {
	  return (HKEY)INVALID_HANDLE_VALUE;
	}
  }
  if (c1 - c == 4) return at;
  LONG err = RegOpenKeyEx(at,_T(""),0,samDesired,&rk);
  if (err == ERROR_SUCCESS) return rk; //Prevent "at" destruction by RegCloseKey
  lastRegErr = err;
  return (HKEY)INVALID_HANDLE_VALUE;
}
//Caller must free handle:
HKEY CreateKeyByName(const TCHAR *c,const TCHAR *cls,REGSAM samDesired) {
  const TCHAR *c1;
  HKEY at = rkeys.KeyByName(c, &c1), rk; DWORD d;
  if (*c1 && at != (HKEY)INVALID_HANDLE_VALUE) {
	if (RegCreateKeyEx(at,c1,0,(TCHAR*)cls,REG_OPTION_NON_VOLATILE,
	  samDesired,NULL,&rk,&d)==ERROR_SUCCESS) return rk;
	else {
	  return (HKEY)INVALID_HANDLE_VALUE;
	}
  }
  if (c1 - c == 4) return at;
  if (RegOpenKeyEx(at,_T(""),0,samDesired,&rk)==ERROR_SUCCESS) return rk; //Prevent "at" destruction by RegCloseKey
  return (HKEY)INVALID_HANDLE_VALUE;
}
int DeleteKeyByName(TCHAR *c) {
  const TCHAR *c1;
  HKEY at = rkeys.KeyByName(c, &c1);
  if (*c1 && at != (HKEY)INVALID_HANDLE_VALUE) {
      return RegDeleteKey(at, c1);
  }
  return !ERROR_SUCCESS;
}
//Caller must free handle:
HKEY GetKeyByItem(HWND tv,HTREEITEM item,REGSAM samDesired) {
  TCHAR *c=GetKeyNameByItem(tv,item);
  HKEY k=GetKeyByName(c,samDesired);
  free(c);
  return k;
}
BOOL CanKeyBeRenamed(HWND tv,HTREEITEM item) {
  TCHAR *c=GetKeyNameByItem(tv,item);
  if (c[4]) {free(c); return 1;}
  free(c);
  return 0;
}
BOOL CloseKey_NHC(HKEY hk) {
  if (hk!=HKEY_CLASSES_ROOT && hk!=HKEY_CURRENT_USER &&
	  hk!=HKEY_LOCAL_MACHINE && hk!=HKEY_USERS &&
#ifndef _WIN32_WCE
	  hk!=HKEY_CURRENT_CONFIG && hk!=HKEY_DYN_DATA && hk!=HKEY_PERFORMANCE_DATA &&
#endif
	  (HANDLE)hk!=INVALID_HANDLE_VALUE)
	  return RegCloseKey(hk);
  else return 0;
}
int RenameKeyValue(HKEY hk, HKEY hk2, const TCHAR *newname, const TCHAR *oldname, bool copy) {
  DWORD vkdl,type;
  if (RegQueryInfoKey(hk,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
    NULL,&vkdl,NULL,NULL)!=ERROR_SUCCESS) return 1;
  if (RegQueryValueEx(hk2,newname,NULL,NULL,NULL,NULL)==ERROR_SUCCESS) {
    return 1;
  }
  char *vd=(char*)malloc(++vkdl+1);
  if (RegQueryValueEx(hk,oldname,NULL,&type,(BYTE*)vd,&vkdl)!=ERROR_SUCCESS ||
    RegSetValueEx(hk2,newname,NULL,type,(BYTE*)vd,vkdl)!=ERROR_SUCCESS) {
    free(vd);
    return 1;
  }
  if (!copy) RegDeleteValue(hk,oldname);
  free(vd);
  return 0;
}
// 0=failure:
BOOL IsSubKey(const TCHAR *what,const TCHAR *of) {
  int w=_tcslen(of);
  if (of[w-1]=='\\') w--;
  return !_tcsncmp(of,what,w) && (!what[w] || what[w]=='\\');
}

//Caller must free memory:
TCHAR *GetKeyNameByItem(HWND tv,HTREEITEM item) {
  TCHAR buf[4096], *rv,*rvpos;
  int posbuf=0;
  TVITEM tvi;
  HTREEITEM hti=item;
  if (hti==NULL) {
	rv=(TCHAR*)malloc(10 * sizeof(*rv));
	_tcscpy(rv,_T("error!(0)"));
	return rv;
  }
  do {
	tvi.mask=TVIF_HANDLE | TVIF_TEXT;
	tvi.hItem=hti;
	tvi.pszText=buf+posbuf, tvi.cchTextMax=4095-posbuf;
	if (!TreeView_GetItem(tv,&tvi)) {ErrMsgDlgBox(_T("GetKeyNameByItem")); break;}
	hti=TreeView_GetParent(tv,hti);
	posbuf+=_tcslen(tvi.pszText)+1;
  } while (hti!=NULL && posbuf<4096);
  if (posbuf>=4096) {
	rv=(TCHAR*)malloc(10 * sizeof(*rv));
	_stprintf(rv,_T("error!(1)"));
	return rv;
  }
  rvpos=rv=(TCHAR*)malloc(posbuf * sizeof(*rv));
  if (!rv) return _T("MEM_ERR");
  for(posbuf--;posbuf>=0;) {
	for(posbuf--;posbuf>=0 && buf[posbuf];posbuf--);
	_tcscpy(rvpos,buf+posbuf+1);
	rvpos+=_tcslen(buf+posbuf+1);
	*(rvpos++)='\\';
  }
  if (rvpos>rv) *(--rvpos)=0;
  return rv;
}

HTREEITEM ShowItemByKeyName(HWND tv, const TCHAR *key) {
  HKEY hk = GetKeyByName(key, KEY_EXECUTE);
  if (hk == (HKEY)INVALID_HANDLE_VALUE)
    return 0;
  CloseKey_NHC(hk);
  achar key2(key);
  TCHAR *c = key2.c;
  HTREEITEM hfc = 0, parent = 0;
  TVITEM tvi;
  TVINSERTSTRUCT tvins;
  TCHAR buf[4096];
  while(c && *c) {
    hfc = TreeView_GetChild(tv, parent);
    TCHAR *d = _tcschr(c, '\\');
    if (d) *d = 0;
    bool already_expanded;
    for(; hfc; hfc = TreeView_GetNextSibling(tv, hfc)) {
      tvi.mask=TVIF_HANDLE | TVIF_TEXT | TVIF_STATE;
      tvi.hItem=hfc;
      tvi.stateMask = TVIS_EXPANDED;
      tvi.pszText = buf, tvi.cchTextMax = 4095;
      if (!TreeView_GetItem(tv, &tvi)) {ErrMsgDlgBox(_T("ShowItemByKeyName")); return 0;}
      if (!_tcsicmp(buf, c)) break;
    }
    if (hfc) already_expanded = (tvi.state & TVIS_EXPANDED) != 0;
    else {
      // as far as the registry key exists...
      tvins.hParent = parent, tvins.hInsertAfter = TVI_SORT;
      tvins.item.mask =  TVIF_TEXT | TVIF_CHILDREN | TVIF_STATE;
      tvins.item.state=0, tvins.item.stateMask=TVIS_EXPANDEDONCE;
      tvins.item.pszText = c;
      tvins.item.cChildren = d != 0;
      hfc = TreeView_InsertItem(tv, &tvins);
      already_expanded = false;
    } 
    if (hfc && !already_expanded && d) {
      TreeView_Expand(tv, hfc, TVE_EXPAND);
    }
    if (d) *d = '\\', c = d + 1;
    else c = 0;
    parent = hfc;
  }
  if (hfc) {
    TreeView_EnsureVisible(tv, hfc);
    TreeView_SelectItem(tv, hfc);
  }
  return hfc;
}

int SelectItemByValueName(HWND lv, const TCHAR *name) {
    LVFINDINFO lvfi; memset(&lvfi, 0, sizeof lvfi);
    lvfi.flags = LVFI_STRING, lvfi.psz = name;
    int n = ListView_FindItem(lv, -1, &lvfi);
    if (n < 0) return n;
    ListView_SetItemState(lv, n, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    return 0;
}

struct key_copy_cookie {
  const TCHAR *dstctrl, *i_kn;
  bool skip_all_err, ignore_err;
  int i_ec;
  key_copy_cookie(const TCHAR *dstctrl_) : dstctrl(dstctrl_), i_kn(0), skip_all_err(0), ignore_err(0), i_ec(0) {}
};

INT_PTR CALLBACK DialogKCE(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  key_copy_cookie *kc = (key_copy_cookie*)GetWindowLongPtr(hwnd, DWLP_USER);
  switch (msg) {
  case WM_INITDIALOG: 
    SetWindowLongPtr(hwnd, DWLP_USER, lParam);
    kc = (key_copy_cookie*)lParam;
    SetDlgItemText(hwnd, IDC_KEYNAME, kc->i_kn);
    if (kc->i_ec != 1) kc->ignore_err = false, EnableWindow(GetDlgItem(hwnd, IDIGNORE), false);
    SendDlgItemMessage(hwnd, IDIGNORE, BM_SETCHECK, kc->ignore_err, 0);
    return 1;
  case WM_CLOSE: EndDialog(hwnd, IDCANCEL); return 1;
  case WM_COMMAND:
    switch (LOWORD (wParam)) {
    case IDOK: EndDialog(hwnd, IDOK); break;
    case IDYES: kc->skip_all_err = true; EndDialog(hwnd, IDOK); break;
    case IDCANCEL: EndDialog(hwnd, IDCANCEL); break; 
    case IDIGNORE: if (kc->i_ec == 1) kc->ignore_err = SendDlgItemMessage(hwnd, IDIGNORE, BM_GETCHECK, 0, 0) != 0; break;
    }
    return 1;
  }
  return 0;
}

static int CopyKeyLight(HKEY src, HKEY dst, key_copy_cookie *cookie) {
  int n=0,k,ec=1;
  HKEY src1,dst1;
  DWORD cknl,ccnl, tnl,tcl, tmp;
  BYTE *cknb,*ccnb;
  FILETIME lwt;
  if (RegQueryInfoKey(src,NULL,NULL,NULL,NULL,&cknl,&ccnl,NULL,0,0,NULL,NULL)!=ERROR_SUCCESS) {
	//todo: error msg
	return 0;
  }
  cknb=(BYTE*)malloc(++cknl); ccnb=(BYTE*)malloc(++ccnl);
  //Copy values:
  value_iterator i(src);
  for(; !i.end(); i++) if (i.is_ok) {
	if (RegSetValueEx(dst, i.name.c, 0, i.type, i.data.b, i.data.l)!=ERROR_SUCCESS) ec|=2;
  }
  if (i.err()) ec|=2;
  //Copy subkeys
  k = 0;
  const TCHAR *const dstctrl = cookie->dstctrl;
  if (dstctrl) for(; dstctrl[k] && dstctrl[k] != '\\'; k++);
  n=0; tnl=cknl, tcl=ccnl;
  while(RegEnumKeyEx(src,n++,(TCHAR*)cknb,&tnl,NULL,(TCHAR*)ccnb,&tcl,&lwt)==ERROR_SUCCESS) {
	if (ec & 4) break;
    const TCHAR *c;
	if(k && !_tcsncmp(dstctrl, (TCHAR*)cknb, k) && !cknb[k]) {
	  c = dstctrl + k;
	  if (*c == '\\') c++;
      if (*c == 0) { tnl = cknl, tcl = ccnl; continue; } //reached dst root!
	} else c = NULL;
	src1 = (HKEY)INVALID_HANDLE_VALUE;
    int srcerr = RegOpenKeyEx(src, (LPCTSTR)cknb, 0, KEY_READ, &src1);
	if (!srcerr &&
		!RegCreateKeyEx(dst,(LPCTSTR)cknb,0,(TCHAR*)ccnb,REG_OPTION_NON_VOLATILE,
		  KEY_WRITE,NULL,&dst1,&tmp)) {
	  cookie->dstctrl = c;
      ec |= CopyKeyLight(src1, dst1, cookie);
	  CloseKey_NHC(dst1);
    } else {
      if (srcerr) {
        cookie->i_kn = (TCHAR*)cknb, cookie->i_ec = ec;
        if (!cookie->skip_all_err && DialogBoxParam(hInst, _T("ASKSRCCOPYFAIL"), MainWindow, DialogKCE, (LPARAM)cookie) == IDCANCEL)
          ec |= 4;
        if (!cookie->ignore_err) ec |= 2;
      } else {
        ec |= 2;
      }
    }
	CloseKey_NHC(src1);
	tnl = cknl, tcl = ccnl;
  }
  cookie->dstctrl = dstctrl; //restore
  free(cknb); free(ccnb);
  return ec;
}

extern bool replace_dstkeyexists_all;
extern HWND RplProgrDlg;
INT_PTR CALLBACK DialogAR (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
bool flag_merge_chosen = false;
// CopyKey:
// retval: 1 = ok, 0 and others = failure;
// act="copy"/"move"
int CopyKey(const TCHAR *src,const TCHAR *dst,const TCHAR *act) {
  HKEY sk,dk;
  int n; DWORD k;
  TCHAR *cls;
  for(n=0;dst[n];n++) if (dst[n]<32) {
	MessageBox(MainWindow,_T("Destination name contains illegal characters"),_T("Error!"),0);
	return 0;
  }
  if (IsSubKey(src,dst) && IsSubKey(dst,src)) {//;)
	TCHAR *s=(TCHAR*)malloc((_tcslen(src)+_tcslen(dst)+40) * sizeof(TCHAR));
	_stprintf(s,_T("Can't %s \n%s\n onto itself!"),act,src);
	MessageBox(MainWindow,s,_T("Error!"),0);
	return 0;
  }
  dk=GetKeyByName((TCHAR*)dst,KEY_WRITE);
  if ((HANDLE)dk!=INVALID_HANDLE_VALUE) {
    CloseKey_NHC(dk);
    if (!replace_dstkeyexists_all || !RplProgrDlg) {
      confirm_replace_dialog_data crd = { src, 0, dst, 0, 0, 0, 0, false };
      int l = DialogBoxParam(hInst, RplProgrDlg? _T("ASKDSTKEYEXISTS") : _T("ASKDSTKEYEXISTS2"),MainWindow,DialogAR, (LPARAM)&crd);
      if (l == 0 || l == 2) {
        if (l == 0 && RplProgrDlg) SendMessage(RplProgrDlg, WM_CLOSE, 0, 0);
        return 0;
      } else {
        if (l == 3 && RplProgrDlg) replace_dstkeyexists_all = true;
        if (!RplProgrDlg) flag_merge_chosen = true;
      }
    }
	//char *s=(char*)malloc(strlen(src)+strlen(dst)+60);
	//sprintf(s,"Can't %s %s onto %s\n because destination already exists!",act,src,dst);
	//MessageBox(MainWindow,s,"Error!",0);
	//return 0;
  }
  sk=GetKeyByName((TCHAR*)src,KEY_READ);
  if ((HANDLE)sk==INVALID_HANDLE_VALUE) {
	TCHAR *s=(TCHAR*)malloc((_tcslen(src)+30) * sizeof(TCHAR));
	_stprintf(s,_T("Can't open key %s"),src);
	MessageBox(MainWindow,s,_T("Error!"),0);
	return 0;
  }
  k=0;
  RegQueryInfoKey(sk,NULL,&k,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
  cls=(TCHAR*)malloc(++k * sizeof(*cls));
  *cls=0;
  RegQueryInfoKey(sk,cls,&k,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
  dk=CreateKeyByName((TCHAR*)dst,cls,KEY_WRITE);
  if ((HANDLE)dk==INVALID_HANDLE_VALUE) {
	TCHAR *s=(TCHAR*)malloc((_tcslen(src)+30) * sizeof(TCHAR));
	_stprintf(s,_T("Can't create key %s"),dst);
	MessageBox(MainWindow,s,_T("Error!"),0);
	CloseKey_NHC(sk); free(cls);
	return 0;
  }
  free(cls);
  if (IsSubKey(dst,src)) {
	cls=(TCHAR*)dst+_tcslen(src);
	if (*cls=='\\') cls++;
  } else cls=_T("");
  key_copy_cookie cookie(cls);
  n = CopyKeyLight(sk, dk, &cookie);
  /*if (n==0 || n==2) {
	//Fatal error
  } else if (n==3) {
	//Non-fatal error
  }*/
  CloseKey_NHC(sk);
  CloseKey_NHC(dk);
  return n;
}

int DeleteAllSubkeys(HKEY src) {
  int ec = 1;
  HKEY src1;
  DWORD cknl, ccnl, tnl, tcl;
  TCHAR *cknb, *ccnb;
  FILETIME lwt;
  if (RegQueryInfoKey(src, NULL, NULL, NULL, NULL, &cknl, &ccnl, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
    //todo: error msg
    return 0;
  }
  cknb = (TCHAR*)malloc(++cknl * sizeof(TCHAR)); ccnb = (TCHAR*)malloc(++ccnl * sizeof(TCHAR));
  tnl = cknl;
  tcl = ccnl;
  int skid = 0;
  while (RegEnumKeyEx(src, skid, cknb, &tnl, NULL, ccnb, &tcl, &lwt) == ERROR_SUCCESS) {
    if (RegOpenKeyEx(src, cknb, 0, KEY_CREATE_SUB_KEY | KEY_READ, &src1) == ERROR_SUCCESS) {
      ec |= DeleteAllSubkeys(src1);
      CloseKey_NHC(src1);
      if (RegDeleteKey(src, cknb) != ERROR_SUCCESS) skid++;
    } else { 
      if (ec & 2) skid++;
      ec |= 2;
    }
    tnl = cknl;
    tcl = ccnl;
  }
  free(cknb);
  free(ccnb);
  return ec;
}

static int DeleteKeyLight(HKEY src,const TCHAR *dstctrl) {
  int k=0,ec=1;
  HKEY src1;
  DWORD cknl,ccnl, tnl,tcl, skinx = 0;
  BYTE *cknb,*ccnb;
  FILETIME lwt;
  if (RegQueryInfoKey(src,NULL,NULL,NULL,NULL,&cknl,&ccnl,NULL,NULL,NULL,NULL,NULL)!=ERROR_SUCCESS) {
	//todo: error msg
	return 0;
  }
  cknb=(BYTE*)malloc(++cknl * sizeof(TCHAR)); ccnb=(BYTE*)malloc(++ccnl * sizeof(TCHAR));
  if (dstctrl) for(;dstctrl[k] && dstctrl[k]!='\\';k++);
  tnl=cknl, tcl=ccnl;
  while(RegEnumKeyEx(src,skinx,(TCHAR*)cknb,&tnl,NULL,(TCHAR*)ccnb,&tcl,&lwt)==ERROR_SUCCESS) {
	const TCHAR *c;
	if(k && !_tcsncmp(dstctrl,(LPCTSTR)cknb,k) && _tcslen((LPCTSTR)cknb)==(DWORD)k) {
	  c=dstctrl+k;
	  if (*c=='\\') c++;
	  if (*c==0) {tnl=cknl, tcl=ccnl; skinx++; continue;} //reached dst root!
	} else c=NULL;
    //m.b. skinx=0
	if (RegOpenKeyEx(src,(LPCTSTR)cknb,0,
		KEY_CREATE_SUB_KEY | KEY_READ,&src1)==ERROR_SUCCESS) {
	  ec|=DeleteKeyLight(src1,c);
	  CloseKey_NHC(src1);
	  RegDeleteKey(src,(LPCTSTR)cknb);
	} else ec|=2;
	tnl=cknl, tcl=ccnl;
  }
  free(cknb); free(ccnb);
  return ec;
}
BOOL DeleteKeyValues(HKEY src) {
  int ec=1,dbg;
  DWORD vknl, tnl;
  BYTE *vknb;
  if (RegQueryInfoKey(src,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&vknl,NULL,NULL,NULL)!=ERROR_SUCCESS) {
	//todo: error msg
	return 0;
  }
  vknb=(BYTE*)malloc(++vknl * sizeof(TCHAR));
  //Copy values:
  tnl=vknl;
  while((dbg=RegEnumValue(src,0,(TCHAR*)vknb,&tnl,NULL,NULL,NULL,NULL))==ERROR_SUCCESS) {
	if (RegDeleteValue(src,(LPCTSTR)vknb)!=ERROR_SUCCESS) ec|=2;
	tnl=vknl;
  }
  free(vknb);
  return ec;
}

const TCHAR *important_keys_list[] = {
  _T("HKLM\\Software"), _T("HKLM\\Software\\Microsoft"), _T("HKLM\\Software\\Microsoft\\Windows"),
  _T("HKLM\\Software\\Microsoft\\Windows NT"),
  _T("HKLM\\Software\\Microsoft\\Windows\\CurrentVersion"), _T("HKLM\\Software\\Microsoft\\Windows NT\\CurrentVersion"),
  _T("HKLM\\HARDWARE"), _T("HKLM\\SAM"), _T("HKLM\\SYSTEM"), _T("HKLM\\SYSTEM\\CurrentControlSet"),
  _T("HKLM\\SYSTEM\\MountedDevices"),
  _T("HKCU\\Software"), _T("HKCU\\Software\\Microsoft"), _T("HKCU\\Software\\Microsoft\\Windows"),
  _T("HKCU\\Software\\Microsoft\\Windows NT"),
  _T("HKCU\\Software\\Microsoft\\Windows\\CurrentVersion"), _T("HKCU\\Software\\Microsoft\\Windows NT\\CurrentVersion"),
  _T("HKCR\\CLSID"), _T("HKLM\\Software\\Classes"), _T("HKLM\\Software\\Classes\\CLSID"),
  0
};
bool allow_delete_important_keys = false;
//slow!!!
bool CanDeleteThisKey(const TCHAR *name, bool qConfig) {
  const TCHAR *c = _tcsrchr(name, '\\');
  const TCHAR *name2 = _tcschr(name, ':'), *nmend = _tcschr(name, '\\');
  nmend = nmend? nmend : name + _tcslen(name);
  if (!name2 || name2 > nmend) name2 = name;
  if (c && !c[1]) {
    for(; c > name2 && c[-1] == '\\'; c--);
  } else
    c = nmend + _tcslen(nmend);
  bool baddel = c == nmend;
  if (!baddel) for(const TCHAR **ikl = important_keys_list; *ikl; ikl++) {
    if (_tcslen(*ikl) == c - name2 && !_tcsnicmp(*ikl, name2, c - name2)) {
      baddel = true;
      break;
    }
  }
  if (!baddel) return true;
  const TCHAR *wstr;
  if (qConfig && !allow_delete_important_keys) {
    MessageBox(MainWindow, _T("You can't delete this key.\nIf you are really sure, turn on \"allow critically important keys deletion\""), name, MB_ICONSTOP);
    return false;
  } else if (qConfig) {
    wstr = _T("You are about to destroy your Windows installation.\n\nAre you sure you want to continue?");
  } else {
    wstr = _T("You should not delete this key.\nOnce this key is deleted, your Windows installation may be destroyed.\n\nAre you sure you want to continue?");
  }
  return MessageBox(MainWindow, wstr, name, MB_OKCANCEL | MB_ICONWARNING) == IDOK;
}

//Be sure that neither src nor dst end with backslash!
int MoveKey(const TCHAR *src, const TCHAR *dst) {
  TCHAR *kc;
  if (!CanDeleteThisKey(src, false)) return 0;
  if (!(kc = (TCHAR*)_tcsrchr(src, '\\'))) return 0;
  int rv = CopyKey(src, dst, _T("move")), n;
  if (rv != 1) {
    MessageBox(MainWindow, _T("Errors happened while copying the key.\nSource key will not be deleted.\n"), _T("Move failed"), MB_OK);
    return rv;
  }
  //Removing src
  TCHAR *cc = (TCHAR*)malloc((_tcslen(src) + 1) * sizeof(*cc));
  for(n = 0; src[n] && src + n != kc; n++) cc[n] = src[n];
  cc[n] = 0;
  auto_close_hkey 
    mk(GetKeyByName(cc,KEY_CREATE_SUB_KEY | KEY_READ)),
    sk(GetKeyByName(src,KEY_CREATE_SUB_KEY | KEY_READ | KEY_SET_VALUE));
  free(cc);
  if ((HANDLE)sk.hk == INVALID_HANDLE_VALUE || (HANDLE)mk.hk == INVALID_HANDLE_VALUE) {
	fchar s(malloc((_tcslen(src) + 30) * sizeof(TCHAR)));
	_stprintf(s.c, _T("Can't delete key %s"), src);
	MessageBox(MainWindow, s.c, _T("Error!"), 0);
	return 0;
  }
  if (!CanDeleteThisKey(src, true)) return 0;
  if (IsSubKey(dst, src)) {
	TCHAR *c;
	if (*(c = (TCHAR*)dst + _tcslen(src)) == '\\') c++;
	DeleteKeyLight(sk.hk, c);
	DeleteKeyValues(sk.hk);
  } else {
	//Straight removing
	DeleteKeyLight(sk.hk, NULL);
	RegDeleteKey(mk.hk, kc+1);
  }
  return rv;
}
