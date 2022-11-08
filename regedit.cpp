#include "regedit.h"
#include "resource.h"
#include <Shlobj.h>

#include "regsavld.h"

HINSTANCE hInst;
HWND MainWindow, SbarW, hwndToolTip, TreeW, ListW, LastFocusedW, RplProgrDlg;
#ifdef _WIN32_WCE
HWND CbarW;
#endif
HCURSOR EWcur;
HIMAGELIST imt;
LRESULT CALLBACK WindowProc (HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK MyHexEditProc (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK EditString (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK EditMString (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK EditBinary (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK EditDWORD (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK DialogAbout (HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK DialogSettings(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK DialogMVCP (HWND,UINT,WPARAM,LPARAM);
#ifndef _WIN32_WCE
INT_PTR CALLBACK DialogCR(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK DialogDcR(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
#endif
INT_PTR CALLBACK DialogGotoKey(HWND,UINT,WPARAM,LPARAM);
INT_PTR CALLBACK DialogAddFavKey(HWND,UINT,WPARAM,LPARAM);

void AddFavoritesToMenu(HMENU);
void AddSelCommandsToMenu(HWND, HMENU);
void CheckDisconnRemoteMenuState();
void DoSearchAndReplace(HWND, bool, bool);
void DoSearchAndReplaceNext(HWND hwnd);
void AddKeyToFavorites(fchar &title, fchar &key, fchar &value, fchar &comment);
void SuggestTitleForKey(const TCHAR *key, fchar &title);
void FavKeyName2ShortName(const TCHAR *k0, fchar &key);
void ValuesAskMoveOrCopy(HWND hwnd, int mvflag, const TCHAR *dst);
void KeyAskMoveOrCopy(HWND hwnd, int mvflag, const TCHAR *dst);
void ValuesContinueDrag(HWND hwnd, LPARAM lParam);
void ValuesEndDrag(HWND hwnd, bool is_ok);

HMENU TypeMenu(DWORD,int,DWORD);
int GetTypeMnuNo(DWORD);
DWORD GetMinRegValueSize(DWORD);
DWORD GetRegValueType(DWORD,DWORD);
int ListValues(HWND,TCHAR*);
int ValueTypeIcon(DWORD);
void GetLVItemText(HWND ListW, int i, TCHAR *&name, DWORD &ns);

int RefreshSubtree(HTREEITEM hfc, const TCHAR *kname, HKEY hk);
int ConnectRegistry(achar &comp, HKEY node, const TCHAR *node_name5, TVINSERTSTRUCT &tvins);
TCHAR szClsName[]=_T("regedit33");
TCHAR szWndName[]=_T("Advanced Registry Editor");

#ifndef _WIN32_WCE
bool has_rest_priv = true, has_back_priv = true;
bool co_initialized = false;
volatile bool rr_connecting = false;
#endif

int dxw=600, dyw=400, xTree=150, xName=150, xData=400, xSplitBar=10;
bool sDat=0, sVal=0, sKeys=0, sMatch=0;
DWORD Settings[16];
DWORD SbarHeight;
#ifdef _WIN32_WCE
DWORD CbarHeight;
#else
DWORD const CbarHeight = 0;
#endif
bool onWpos;
int xWpos;
TCHAR *currentitem=NULL;
HTREEITEM currentitem_tv=NULL;
HBITMAP img_up,img_down;
HFONT Cour12;
HPEN ThickPen;
HMENU theFavMenu = NULL, theSelMenu = NULL, theFileMenu = NULL, theEditMenu = NULL;
struct favitem_t { TCHAR *name, *key, *value, *comment; };
static vector<favitem_t> favItems;
#define REFAVPATH _T("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit\\Favorites")
#define MYCOMP _T("My Computer\\")
#define MYCOMPLEN 12

int dragitno = 0;
HIMAGELIST dragiml = 0;
bool is_dragging = false, is_key_dragging = false; 
bool prev_candrop = false, could_ever_drop = false;
HTREEITEM prevdhlti = 0;
// TreeView_SetItemState is documented as 5.80, but it doesn't use anything!!!
#ifndef TreeView_SetItemState
#define TreeView_SetItemState(hwndTV, hti, data, _mask) \
{ TVITEM _ms_TVi;\
  _ms_TVi.mask = TVIF_STATE; \
  _ms_TVi.hItem = hti; \
  _ms_TVi.stateMask = _mask;\
  _ms_TVi.state = data;\
  SNDMSG((hwndTV), TVM_SETITEM, 0, (LPARAM)(TV_ITEM FAR *)&_ms_TVi);\
}
#endif
HCURSOR curs_arr = 0, curs_no = 0;
//HICON regsmallicon = 0;
DWORD prevdhltibtm = 0;

TCHAR *mvcpkeyfrom = 0, *mvcpkeyto = 0;
BYTE mvcp_move = 1;

struct connect_remote_dialog_data {
  bool LM, CR, US, PD, DD;
  achar comp;
};
struct disconnect_remote_dialog_data {
  int numsel;
  const TCHAR **keys;
  disconnect_remote_dialog_data() : numsel(0), keys(0) {}
  ~disconnect_remote_dialog_data() { free(keys); }
};

static HTREEITEM HKCR,HKCU,HKLM,HKUS,HKCC,HKDD,HKPD;

int WINAPI _tWinMain(HINSTANCE hTI, HINSTANCE, LPTSTR lpszargs, int nWinMode) {
  HWND hwnd;
  MSG msg;
  WNDCLASS wcl;
  hInst=hTI;

  if (FindWindow(szClsName,NULL)) {MessageBeep(MB_OK);return 0;}//?
  InitCommonControls();
  
  wcl.hInstance=hTI;
  wcl.lpszClassName=szClsName;
  wcl.lpfnWndProc=WindowProc;
  wcl.style=CS_HREDRAW | CS_VREDRAW;
  wcl.hIcon=LoadIcon (hTI,_T("RegEdit"));
  wcl.hCursor=LoadCursor(NULL,IDC_SIZEWE);//LoadCursor (NULL, IDC_ARROW);
#ifndef _WIN32_WCE
  wcl.lpszMenuName=_T("MainMenu");
#else
  wcl.lpszMenuName=NULL;
#endif
  wcl.cbClsExtra=0;
  wcl.cbWndExtra=0;
  wcl.hbrBackground=(HBRUSH)GetStockObject (LTGRAY_BRUSH);
  if (!RegisterClass (&wcl)) return 0;

  wcl.lpszClassName=_T("MyHexEdit");
  wcl.lpfnWndProc=MyHexEditProc;
  wcl.hIcon=NULL, wcl.hCursor=NULL;
  wcl.lpszMenuName=NULL;
  wcl.cbWndExtra=16;
  wcl.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
  if (!RegisterClass (&wcl)) return 0;

  EWcur=LoadCursor(NULL,IDC_SIZEWE);
  imt=ImageList_LoadBitmap(hTI,MAKEINTRESOURCE(IDB_TYPES),16,0,CLR_NONE);
  img_up=LoadBitmap(hTI,_T("ARROWUP"));
  img_down=LoadBitmap(hTI,_T("ARROWDOWN"));
  static LOGFONT const lf = {
    16, 8, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, _T("Courier New")
  };
  Cour12=CreateFontIndirect(&lf);
  ThickPen=CreatePen(PS_SOLID,3,RGB(0,0,0));

  curs_no = LoadCursor(NULL, IDC_NO);
  if (!curs_no) ErrMsgDlgBox(_T("LoadCursor(NULL, IDC_NO)"));
  curs_arr = LoadCursor(NULL, IDC_ARROW);
  if (!curs_arr) ErrMsgDlgBox(_T("LoadCursor(NULL, IDC_ARROW)"));
  //regsmallicon = LoadIcon(hInst, _T("REGSMALL"));


  //Beep(300,10);
  LoadSettings();
  hwnd=CreateWindow (szClsName, szWndName,
	WS_OVERLAPPEDWINDOW,
	CW_USEDEFAULT,
	CW_USEDEFAULT,
	CW_USEDEFAULT, //Width
	CW_USEDEFAULT, //Height
	HWND_DESKTOP,
	NULL,
	hTI,
	NULL
    );
  MainWindow=hwnd;

  ShowWindow(hwnd,nWinMode);
  UpdateWindow(hwnd);

#ifdef _WIN32_WCE
  HMENU theMenu = CommandBar_GetMenu(CbarW, 0);
#else 
  HMENU theMenu = GetMenu(hwnd);
#endif
  theFavMenu = GetSubMenu(theMenu, 3);
  theSelMenu = GetSubMenu(theMenu, 4);
  theFileMenu = GetSubMenu(theMenu, 0);
  theEditMenu = GetSubMenu(theMenu, 1);

#ifndef _WIN32_WCE
  if (EnablePrivilege_NT(0, SE_BACKUP_NAME)) {
    has_back_priv = false;
    //ErrMsgDlgBox(SE_BACKUP_NAME);
  }
  if (EnablePrivilege_NT(0, SE_RESTORE_NAME)) {
    has_rest_priv = false;
    //ErrMsgDlgBox(SE_RESTORE_NAME);
  }
  EnablePrivilege_NT(0, SE_SHUTDOWN_NAME); //may be...
#endif

  while (GetMessage (&msg,NULL,0,0)) {
	TranslateMessage (&msg);
    DispatchMessage(&msg);
  }
  //CloseHandle(imt);
  DeleteObject(img_up); DeleteObject(img_down);
  DeleteObject(Cour12); DeleteObject(ThickPen);

#ifndef _WIN32_WCE
  if (co_initialized) CoUninitialize();
#endif

  return 0;
}

LRESULT CALLBACK WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  //HDC hdc;
  int n,k,i;
  TCHAR *s,ss[40];
  HKEY hk;
  RECT rc;
  DWORD mp;
  POINT pt;
  TVINSERTSTRUCT tvins;
  LVCOLUMN lvcol;
  HMENU pum;

  switch (msg) {
  case WM_CHAR:
	if ((TCHAR)wParam==9 && !is_dragging) {
	  if (LastFocusedW==TreeW) LastFocusedW=ListW;
	  else LastFocusedW=TreeW;
	  SetFocus(LastFocusedW);
	  //Beep(500,50);
	}
    if ((TCHAR)wParam == 27 && is_dragging) {
      ValuesEndDrag(hwnd, false);
    }
	break;
  //case WM_PAINT:
	//break;

  case WM_COMMAND:
    if (LOWORD(wParam) >= 41000 && LOWORD(wParam) < 41100) { //Favorites menu
      size_t it = LOWORD(wParam) - 41000;
      if (it >= favItems.size()) break;
      TCHAR *k0 = favItems[it].key; fchar key;
      FavKeyName2ShortName(k0, key);
      HTREEITEM ti = ShowItemByKeyName(TreeW, key.c);
      if (ti && favItems[it].value) {
        if (!SelectItemByValueName(ListW, favItems[it].value))
          SetFocus(LastFocusedW = ListW);
      }
      break;
    }
    switch (LOWORD (wParam)) {
	case IDM_EXIT:
	  //SaveSettings();
	  DestroyWindow(hwnd);
	  break;
	  
	case IDM_SETTINGS:
	  DialogBox(hInst, _T("SETTINGS"), hwnd, DialogSettings);
	  break;
	  
	case IDM_ABOUT:
	  DialogBox(hInst, _T("ABOUT"), hwnd, DialogAbout);
	  break;

	case IDM_EDIT_FIND:
	case IDM_SEARCHREPLACE:
      if (RplProgrDlg) { SetFocus(RplProgrDlg); break; }
      DoSearchAndReplace(hwnd, false, LOWORD(wParam) == IDM_EDIT_FIND);
	  break;

    case IDM_FINDNEXT:
      DoSearchAndReplaceNext(hwnd);
      break;

    case IDM_IMPORTREGFILE:
      {
        achar tname(_T(""));
        if (DisplayOFNdlg(tname, _T("Choose .reg - file"), _T("Regedit files\0*.reg\0All files\0*.*\0"))) {
          LoadDump(tname.c);
          SendMessage(hwnd,WM_COMMAND,340,0);
        }
      }
      break;

    case IDM_EXPORTREGFILE:
      {
        achar tname(_T(""));
        if (int choice = DisplayOFNdlg(tname, _T("Choose .reg - file"), _T("Regedit4 files\0*.reg\0Regedit5 files\0*.reg\0"), true, true)) {
          const TCHAR *fknt;
          const TCHAR *fknr = rkeys.ShortName2LongName(currentitem, &fknt);
          int len = _tcslen(fknr) + _tcslen(fknt) + (*fknt != 0);
          achar key(len * sizeof(TCHAR));
          if (*fknt) _stprintf(key.c, _T("%s\\%s"), fknr, fknt);
          else _tcscpy(key.c, fknr);
          SaveDump(tname.c, key.c, choice == 2);
        }
      }
      break;

#ifndef _WIN32_WCE
    case IDM_REGISTRY_CONNECT: {
        if (rr_connecting) {
          MessageBox(hwnd, _T("Previous connection has not been completed yet"), _T("Connect network registry"), MB_ICONSTOP);
          break;
        }
        connect_remote_dialog_data d;
	    tvins.hParent=TVI_ROOT, tvins.hInsertAfter=TVI_LAST;
	    tvins.item.mask=TVIF_CHILDREN | TVIF_STATE | TVIF_TEXT, tvins.item.state=TVIS_BOLD;//|TVIS_EXPANDED ; 
	    tvins.item.stateMask=0xFFFF, tvins.item.cChildren=1;
        if (DialogBoxParam(hInst,_T("CONNREMOTE"),hwnd,DialogCR,(LPARAM)&d)) {
          if (d.LM && ConnectRegistry(d.comp, HKEY_LOCAL_MACHINE, _T(":HKLM"), tvins)) break;
          if (d.US) ConnectRegistry(d.comp, HKEY_USERS, _T(":HKUS"), tvins);
          if (d.DD) ConnectRegistry(d.comp, HKEY_DYN_DATA, _T(":HKDD"), tvins);
          if (d.PD) ConnectRegistry(d.comp, HKEY_PERFORMANCE_DATA, _T(":HKPD"), tvins);
          CheckDisconnRemoteMenuState();
        }
      }
      break;

    case IDM_REGISTRY_DISCONNECT: {
      disconnect_remote_dialog_data d; size_t n, k;
      for(n = k = 0; n < rkeys.v.size(); n++) if (rkeys.v[n].ki.flags & IS_REMOTE_KEY) k++;
      d.keys = (const TCHAR**)malloc((d.numsel = k) * sizeof(TCHAR*));
      for(n = k = 0; n < rkeys.v.size(); n++) if (rkeys.v[n].ki.flags & IS_REMOTE_KEY) d.keys[k++] = rkeys.v[n].name;
      if (DialogBoxParam(hInst, _T("DISCONNREMOTE"), hwnd, DialogDcR,(LPARAM)&d)) {
        for(n = 0; n < k; n++) if (d.keys[n]) {
          HKRootName2Handle_item_type out;
          rkeys.remove(d.keys[n], out);
          TreeView_DeleteItem(TreeW, out.ki.item);
          RegCloseKey(out.ki.hkey);
          free((void*)out.name);
        }
        CheckDisconnRemoteMenuState();
      }
      break;
    }

    case IDM_REGISTRY_LOADHIVE: {
        load_hive_dialog_data d;
        achar ci = currentitem;
        TCHAR *c = _tcschr(ci.c, '\\'); if (c) *c = 0;
        d.root_key_name = ci.c;
        if (DialogBoxParam(hInst,_T("LOADHIVE"),hwnd,DialogLDH,(LPARAM)&d)) {
          hk = rkeys.KeyByName(d.root_key_name);
          if(DWORD LE=RegLoadKey(hk, d.subkey_name.c, d.fname.c)) {
            ErrMsgDlgBox(_T("RegLoadKey"), LE);
          } else {
            SendMessage(hwnd,WM_COMMAND,340,0);
          }
        }
      }
      break;

    case IDM_REGISTRY_UNLOADHIVE: {
        achar ci = currentitem;
        TCHAR *c = _tcschr(ci.c, '\\'); if (!c) break;
        *c++ = 0;
        hk = rkeys.KeyByName(ci.c);
        //MessageBox(hwnd, ci, c, 0);
        if (DWORD LE=RegUnLoadKey(hk, c)) {
          ErrMsgDlgBox(_T("RegUnLoadKey"), LE);
        } else
          SendMessage(hwnd,WM_COMMAND,340,0);
      }
      break;

    case ID_EDIT_SAVEKEYTOFILE: {
        save_key_dialog_data d;
        d.key_name = currentitem;
        if (DialogBoxParam(hInst,_T("SAVEKEY"),hwnd,DialogSVK,(LPARAM)&d)) {
          hk = GetKeyByName(d.key_name.c, KEY_READ);
          if(DWORD LE=RegSaveKey(hk, d.fname.c, NULL)) {
            ErrMsgDlgBox(_T("RegSaveKey"), LE);
          } else {
            SendMessage(hwnd,WM_COMMAND,340,0);
          }
          RegCloseKey(hk);
        }
      }
      break;

    case ID_EDIT_LOADKEYFROMFILE: {
        load_key_dialog_data d;
        d.key_name = currentitem;
        d.force = d.nolazy = d.refresh = d.volatil = false;
        if (DialogBoxParam(hInst,_T("LOADKEY"),hwnd,DialogLDK,(LPARAM)&d)) {
          hk = GetKeyByName(d.key_name.c, KEY_READ);
          DWORD flag = (d.force? REG_FORCE_RESTORE : 0) | (d.nolazy? REG_NO_LAZY_FLUSH : 0)
            | (d.refresh? REG_REFRESH_HIVE : 0) | (d.volatil? REG_WHOLE_HIVE_VOLATILE : 0);
          if(DWORD LE=RegRestoreKey(hk, d.fname.c, flag)) {
            ErrMsgDlgBox(_T("RegRestoreKey"), LE);
          } else {
            SendMessage(hwnd,WM_COMMAND,340,0);
          }
          RegCloseKey(hk);
        }
      }
      break;

    case ID_EDIT_REPLACEKEY: {
        replace_key_dialog_data d;
        d.key_name = currentitem;
        if (DialogBoxParam(hInst,_T("REGREPLACEKEY"),hwnd,DialogRplK,(LPARAM)&d)) {
          hk = GetKeyByName(d.key_name.c, KEY_READ);
          if(DWORD LE=RegReplaceKey(hk, NULL, d.fname_new.c, d.fname_old.c)) {
            ErrMsgDlgBox(_T("RegReplaceKey"), LE);
          } else {
            SendMessage(hwnd,WM_COMMAND,340,0);
          }
          RegCloseKey(hk);
        }
      }
      break;
#endif

    case IDM_FAV_GOTO: {
        fchar key;
        if (DialogBoxParam(hInst,_T("GOTOKEY"),hwnd,DialogGotoKey,(LPARAM)&key)) {
          ShowItemByKeyName(TreeW, key.c);
        }
      }
      break;

    case IDM_FAV_ADDKEY: {
        fchar key_and_title[4]; key_and_title[0].c = _tcsdup(currentitem);
        SuggestTitleForKey(key_and_title[0].c /*key*/, key_and_title[1] /*title*/);
        i = ListView_GetNextItem(ListW, -1, LVNI_FOCUSED);
        if (i >= 0) {
          DWORD ns = 0;
          GetLVItemText(ListW, i, key_and_title[2].c, ns);
        }
        if (DialogBoxParam(hInst,_T("ADDFAVKEY"),hwnd,DialogAddFavKey,(LPARAM)&key_and_title)) {
          AddKeyToFavorites(key_and_title[1], key_and_title[0], key_and_title[2], key_and_title[3]);
        }
      }
      break;

    case IDM_FAV_EDIT: {
      bool is_already_there = !_tcsicmp(currentitem, _T("HKCU\\") REFAVPATH);
      ShowItemByKeyName(TreeW, _T("HKCU\\") REFAVPATH);
	  LastFocusedW=ListW;
	  SetFocus(LastFocusedW);
      if (!is_already_there && ListView_GetItemCount(ListW) > 0)  //And select first item (if any)
        ListView_SetItemState(ListW, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
      }
      break;

    case 340: {//Refresh key tree
        HTREEITEM hfc = TreeView_GetChild(TreeW, 0);
        for(n = 0; hfc; n++) {
          TCHAR *kname = GetKeyNameByItem(TreeW, hfc);
          hk = GetKeyByName(kname, KEY_ENUMERATE_SUB_KEYS);
          TVITEM tvi;
          tvi.mask=TVIF_HANDLE | TVIF_STATE, tvi.hItem=hfc, tvi.stateMask = TVIS_EXPANDED;
          if (!TreeView_GetItem(TreeW, &tvi)) {ErrMsgDlgBox(_T("RefreshSubtree")); return 1;}
          bool is_exp = !!(tvi.state & TVIS_EXPANDED);
          if ((HANDLE)hk != INVALID_HANDLE_VALUE && is_exp) //?!
            RefreshSubtree(hfc, kname, hk);
          free(kname);
          RegCloseKey(hk);
          hfc = TreeView_GetNextSibling(TreeW, hfc);
        }
      }
	  break;

	case 320:
	  if (!ListView_GetSelectedCount(ListW)) break;
	  i = ListView_GetNextItem(ListW, -1, LVNI_FOCUSED);
	  goto e201dbl;
	  break;

	case 321://Delete value(s)
	  k=ListView_GetSelectedCount(ListW);
	  if (currentitem && k) {
		NMLVKEYDOWN nmdel;
		nmdel.hdr.code=LVN_KEYDOWN, nmdel.hdr.idFrom=201, nmdel.hdr.hwndFrom=ListW;
		nmdel.wVKey=VK_DELETE, nmdel.flags=0;
		SendMessage(hwnd,WM_NOTIFY,201,(LPARAM)&nmdel);
	  }
	  break;
	case 322://Rename value
	  k = ListView_GetNextItem(ListW, -1, LVNI_FOCUSED);
	  if (k>=0) ListView_EditLabel(ListW,k);
	  break;

    case 323://move value(s)
    case 324://copy value(s)
      ValuesAskMoveOrCopy(hwnd, (int)(LOWORD (wParam) == 323) | 2, 0);
      break;

	case 329: if (currentitem) TreeView_Expand(TreeW,currentitem_tv,TVE_COLLAPSE | TVE_COLLAPSERESET);
	  break;
	case 330: if (currentitem) {
		tvins.item.mask=TVIF_CHILDREN | TVIF_STATE;
		tvins.item.hItem=currentitem_tv; tvins.item.cChildren=1;
		tvins.item.state=0, tvins.item.stateMask=TVIS_EXPANDEDONCE;
		TreeView_SetItem(TreeW,&tvins.item);
		TreeView_Expand(TreeW,currentitem_tv,TVE_EXPAND);
	  }
	  break;
	case 332://delete key
	  if (currentitem) {
		NMTVKEYDOWN nmdel;
		nmdel.hdr.code=TVN_KEYDOWN, nmdel.hdr.idFrom=200, nmdel.hdr.hwndFrom=TreeW;
		nmdel.wVKey=VK_DELETE, nmdel.flags=0;
		SendMessage(hwnd,WM_NOTIFY,200,(LPARAM)&nmdel);
	  }
	  break;
	case 333://rename key
	  TreeView_EnsureVisible(TreeW,currentitem_tv);
	  TreeView_EditLabel(TreeW,currentitem_tv);
	  TreeView_Select(TreeW,currentitem_tv,TVGN_CARET);
	  break;

	case 334://move key
    case 335://copy key
	  KeyAskMoveOrCopy(hwnd, LOWORD (wParam) == 334, 0);
	  break;
    
    case 336://copy key name
    case 337://copy key name (short)
      if (*currentitem) {
        if (!OpenClipboard(hwnd)) break;
        bool full = LOWORD (wParam) == 336;
        const TCHAR *fknr, *fknt;
        int len = _tcslen(currentitem) + 1;
        if (full) {
          fknr = rkeys.ShortName2LongName(currentitem, &fknt);
          if (fknr) len = _tcslen(fknr) + _tcslen(fknt) + 1 + (*fknt != 0);
          else full = false;
        }
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(TCHAR));
        if (g) {
          TCHAR *c = (TCHAR*)GlobalLock(g);
          if (c) {
            if (full) 
              if (*fknt) _stprintf(c, _T("%s\\%s"), fknr, fknt);
              else _tcscpy(c, fknr);
            else _tcscpy(c, currentitem);
            GlobalUnlock(g);
            EmptyClipboard();
            SetClipboardData(sizeof(TCHAR) == sizeof(WCHAR) ? CF_UNICODETEXT : CF_TEXT, g);
          }
        }
        CloseClipboard(); 
      }
      break;

	case 300://Create new subkey
	  if (!currentitem) break;
	  hk=GetKeyByName(currentitem,KEY_CREATE_SUB_KEY);
	  if((HANDLE)hk==INVALID_HANDLE_VALUE) {
		MessageBox(hwnd,_T("Can't create subkeys for this key"),currentitem,0);
		break;
	  }
	  tvins.item.mask=TVIF_CHILDREN | TVIF_STATE;
	  tvins.item.hItem=currentitem_tv; tvins.item.cChildren=1;
	  tvins.item.state=0, tvins.item.stateMask=TVIS_EXPANDEDONCE;
	  TreeView_SetItem(TreeW,&tvins.item);
	  TreeView_Expand(TreeW,currentitem_tv,TVE_EXPAND);
	  n=0;
	  do {
		HKEY hk1;
		_stprintf(ss,_T("New key #%03i"),n++);
		if (RegCreateKeyEx(hk,ss,NULL,_T("regedt"),REG_OPTION_NON_VOLATILE,
		  KEY_QUERY_VALUE,NULL,&hk1,(DWORD*)&k)!=ERROR_SUCCESS) k=-1;
		else RegCloseKey(hk1);
	  } while(k==REG_OPENED_EXISTING_KEY);
	  if (k==-1) {RegCloseKey(hk); break;}
	  s=(TCHAR*)malloc((_tcslen(currentitem)+_tcslen(ss)+5) * sizeof(TCHAR));
	  _stprintf(s,_T("%s\\%s"),currentitem,ss);
	  free(currentitem); currentitem=s;
	  //SendMessage(hwnd,WM_COMMAND,340,1);
	  RegCloseKey(hk);
	  tvins.hParent=currentitem_tv, tvins.hInsertAfter=TVI_SORT;
	  tvins.item.mask=TVIF_CHILDREN | TVIF_STATE | TVIF_TEXT, tvins.item.state=0; 
	  tvins.item.stateMask=0xFFFF, tvins.item.cChildren=1;
	  tvins.item.pszText=ss, tvins.item.cChildren=0;
	  currentitem_tv=TreeView_InsertItem(TreeW,&tvins);
	  TreeView_EnsureVisible(TreeW,currentitem_tv);
	  TreeView_EditLabel(TreeW,currentitem_tv);
	  TreeView_Select(TreeW,currentitem_tv,TVGN_CARET);
	  break;
	
	case 301:case 302:case 303:case 304:case 305:case 306:case 307://Create new value
	case 308:case 309:case 310:case 311:case 312:
	  if (!currentitem) break;
	  hk=GetKeyByName(currentitem,KEY_SET_VALUE | KEY_QUERY_VALUE);
	  if((HANDLE)hk==INVALID_HANDLE_VALUE) {
		MessageBox(hwnd,_T("Can't create values for this key"),currentitem,0);
		break;
	  }
	  n=0;
	  do {
		_stprintf(ss,_T("New Value #%03i"),n++);
	  } while(RegQueryValueEx(hk,ss,NULL,NULL,NULL,NULL)==ERROR_SUCCESS);
	  n--;
	  k=GetRegValueType(LOWORD(wParam)-300,12);
	  SetFocus(ListW);
	  RegSetValueEx(hk,ss,NULL,k,(BYTE*)"\0\0\0",GetMinRegValueSize(k));
	  RegCloseKey(hk);
	  ListValues(hwnd,currentitem);
	  {
		LVFINDINFO finf;
		finf.flags=LVFI_STRING, finf.psz=ss;
		k=ListView_FindItem(ListW,-1,&finf);
		if (k>=0) ListView_EditLabel(ListW,k);
	  }
	  break;

	case 351:case 352:case 353:case 354:case 355:case 356:case 357:
	case 358:case 359:case 360:case 361:case 362:
	  if (currentitem) {
		if (!ListView_GetSelectedCount(ListW)) break;
		i = ListView_GetNextItem(ListW, -1, LVNI_FOCUSED);
		fchar name;
		DWORD ns=0;
		GetLVItemText(ListW, i, name.c, ns);
		TCHAR *vn,*vdp;
		char *vd;
		DWORD vknl,vkdl,type;
		LVITEM item;
		hk=GetKeyByName(currentitem,KEY_QUERY_VALUE | KEY_SET_VALUE);
		if ((HANDLE)hk==INVALID_HANDLE_VALUE) return 0;
		if (RegQueryInfoKey(hk,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
		  &vknl,&vkdl,NULL,NULL)!=ERROR_SUCCESS) return 0;
		vn=(TCHAR*)malloc((++vknl+20) * sizeof(TCHAR)); vd=(char*)malloc(++vkdl+1);
		if (RegQueryValueEx(hk,name.c,NULL,&type,(BYTE*)vd,&vkdl)!=ERROR_SUCCESS ||
			RegSetValueEx(hk,name.c,NULL,
			type=GetRegValueType(LOWORD(wParam)-350,type),
			(BYTE*)vd,vkdl)!=ERROR_SUCCESS) {
		  RegCloseKey(hk);
		  free(vn); free(vd);
		  SetFocus(ListW);
		  return 0;
		}
		SetFocus(ListW);
		RegCloseKey(hk);
		vdp=(TCHAR*)malloc(max<int>(vkdl*3,32));
		item.iItem=i, item.mask=LVIF_IMAGE, item.iSubItem=0;
		item.stateMask=0, item.iImage=ValueTypeIcon(type);
		ListView_SetItem(ListW,&item);
		GetValueDataString(vd,vdp,vkdl,type);
		item.mask=LVIF_TEXT, item.iSubItem=1;
		item.pszText=vdp;
		ListView_SetItem(ListW,&item);
		free(vn); free(vd); free(vdp);
	  }
	  break;
	}
	break;
	
  case WM_CREATE:
	//MyFnt=CreateFont(16,8,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_DONTCARE, "Arial Cyr");
	//MyFnS=CreateFont(16,8,0,0,FW_LIGHT,FALSE,FALSE,TRUE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_DONTCARE, "Arial Cyr");
	//DlgThinFnt=CreateFont(15,5,0,0,FW_LIGHT,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_DONTCARE, "Arial Cyr");
	//Sans8=CreateFont(16,7,0,0,FW_LIGHT,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");
	GetClientRect(hwnd, &rc);
	xTree = MulDiv(rc.right - rc.left, xTree, dxw);
	xName = MulDiv(rc.right - rc.left, xName, dxw);
	xData = MulDiv(rc.right - rc.left, xData, dxw);
#ifdef _WIN32_WCE
	CbarW = CommandBar_Create(hInst, hwnd, 1);
	CommandBar_InsertMenubarEx(CbarW, hInst, _T("MainMenu"), 0);
	CommandBar_Show(CbarW, TRUE);
	CbarHeight = CommandBar_Height(CbarW);
#endif
	SbarW = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_BORDER | SBARS_SIZEGRIP, _T("Root"), hwnd, 1001);
	hwndToolTip = CreateWindowEx( 0, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, 20, 20, hwnd, NULL, hInst, NULL);
	if (!hwndToolTip) MessageBeep(MB_OK);
    else SendMessage(hwndToolTip, TTM_SETMAXTIPWIDTH, 0, 300);
	SendMessage(SbarW, SB_GETRECT, 0, (LPARAM)&rc);
	SbarHeight = rc.bottom - rc.top;
	TreeW=CreateWindowEx(WS_EX_CLIENTEDGE,WC_TREEVIEW,_T("none"), 
	  WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT |
	  TVS_EDITLABELS | TVS_SHOWSELALWAYS | WS_TABSTOP,
	  0,0,0,0,hwnd,(HMENU)200,hInst,NULL);
	if (!TreeW) ErrMsgDlgBox(_T("TreeView"));
	LastFocusedW=TreeW;
	ListW=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,_T("none"), 
	  WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHAREIMAGELISTS | WS_TABSTOP |
	  LVS_EDITLABELS | LVS_SORTASCENDING,
	  0,0,0,0,hwnd,(HMENU)201,hInst,NULL);
	if (!ListW) ErrMsgDlgBox(_T("SysListView32"));
	ListView_SetImageList(ListW,imt,LVSIL_SMALL);
	//TreeView Setup
	tvins.hParent=TVI_ROOT, tvins.hInsertAfter=TVI_LAST;
	tvins.item.mask=TVIF_CHILDREN | TVIF_STATE | TVIF_TEXT, tvins.item.state=TVIS_BOLD;//|TVIS_EXPANDED ; 
	tvins.item.stateMask=0xFFFF, tvins.item.cChildren=1;
	tvins.item.pszText=_T("HKCR");
	HKCR=TreeView_InsertItem(TreeW,&tvins);
	tvins.item.pszText=_T("HKCU");
	HKCU=TreeView_InsertItem(TreeW,&tvins);
	tvins.item.pszText=_T("HKLM");
	HKLM=TreeView_InsertItem(TreeW,&tvins);
	tvins.item.pszText=_T("HKUS");
	HKUS=TreeView_InsertItem(TreeW,&tvins);
#ifndef _WIN32_WCE
	tvins.item.pszText=_T("HKCC");
	HKCC=TreeView_InsertItem(TreeW,&tvins);
	tvins.item.pszText=_T("HKDD");//Win95
	HKCC=TreeView_InsertItem(TreeW,&tvins);
	tvins.item.pszText=_T("HKPD");//WinNT
	HKCC=TreeView_InsertItem(TreeW,&tvins);
#endif
	//ListView Setup
	lvcol.mask=LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
	lvcol.fmt=LVCFMT_LEFT;
	lvcol.iSubItem = 0; lvcol.pszText = _T("Name"); lvcol.cx = xName;
	if (ListView_InsertColumn(ListW,0,&lvcol)==-1) ErrMsgDlgBox(_T("ListView"));
	lvcol.iSubItem = 1; lvcol.pszText = _T("Data"); lvcol.cx = xData;
	if (ListView_InsertColumn(ListW,1,&lvcol)==-1) ErrMsgDlgBox(_T("ListView"));
	SetFocus(TreeW);
	break;
	  
  case WM_CLOSE:
	//SaveSettings();
    if (RplProgrDlg) { SetFocus(RplProgrDlg); break; }
	DestroyWindow(hwnd);
	break;
	
  case WM_DESTROY:
	//DeleteObject(MyFnt); DeleteObject(MyFnS);
	//DeleteObject(DlgThinFnt); //DeleteObject(Sans8);
#ifdef _WIN32_WCE
	if (CbarW) CommandBar_Destroy(CbarW);
#endif
	PostQuitMessage (0);
	break;

  case WM_INITMENUPOPUP:
    if ((HMENU)wParam == theFavMenu) {
      AddFavoritesToMenu((HMENU)wParam);
      return 0;
    }
    if ((HMENU)wParam == theSelMenu) {
      AddSelCommandsToMenu(GetFocus(), (HMENU)wParam);
      return 0;
    }
    break;
	
  case WM_NOTIFY:
	k=((LPNMHDR)lParam)->idFrom;
	switch (((LPNMHDR)lParam)->code) {
      unsigned short wVKey;
	case TTN_NEEDTEXT:
	  break;

	case TVN_SELCHANGING:
	  if (is_dragging) ValuesEndDrag(hwnd, false);
      /*if (k==200)*/ return 0;
	  break;
	case TVN_SELCHANGED:
	  if (k==200) {
		s=GetKeyNameByItem(TreeW,currentitem_tv=((LPNMTREEVIEW)lParam)->itemNew.hItem);
		if (currentitem) free(currentitem);
		currentitem=s;
		SendMessage(SbarW,SB_SETTEXT,0,(LPARAM)s);
		ListValues(hwnd,s);
	  }
	  break;

	case TVN_ITEMEXPANDING:
	  if (k==200 && ((LPNMTREEVIEW)lParam)->action==TVE_EXPAND &&
		!(((LPNMTREEVIEW)lParam)->itemNew.state&TVIS_EXPANDED)) {
		TCHAR s[512],ss[512],sss[512];
		DWORD d=512;
		FILETIME lwt;
		HKEY hks;
		HTREEITEM hdel=TreeView_GetChild(TreeW,((LPNMTREEVIEW)lParam)->itemNew.hItem);
		sss[0]=0;
		if (hdel) {
		  tvins.item.mask=TVIF_TEXT, tvins.item.pszText=sss, tvins.item.cchTextMax=512;
		  tvins.item.hItem=hdel;
		  TreeView_GetItem(TreeW,&tvins.item);
		}
		tvins.hParent=((LPNMTREEVIEW)lParam)->itemNew.hItem, tvins.hInsertAfter=TVI_LAST;
		tvins.item.mask=TVIF_CHILDREN | TVIF_STATE | TVIF_TEXT, tvins.item.state=0; 
		tvins.item.stateMask=0xFFFF, tvins.item.cChildren=1;
		hk=GetKeyByItem(TreeW,tvins.hParent,KEY_ENUMERATE_SUB_KEYS);
		tvins.item.pszText=s;
		n=0;
		while (RegEnumKeyEx(hk,n++,s,&d,NULL,NULL,NULL,&lwt)==ERROR_SUCCESS) {
		  tvins.item.cChildren=0;
		  if (RegOpenKeyEx(hk,s,0,KEY_READ,&hks)==ERROR_SUCCESS) {
		    d=512;
		    if (RegEnumKeyEx(hks,0,ss,&d,NULL,NULL,NULL,&lwt)==ERROR_SUCCESS) {
		      tvins.item.cChildren=1;
		    }
		    CloseKey_NHC(hks);
		  }
		  if (_tcscmp(sss,s)) TreeView_InsertItem(TreeW,&tvins);
		  d=512;
		}
		CloseKey_NHC(hk);
		TreeView_SortChildren(TreeW,((LPNMTREEVIEW)lParam)->itemNew.hItem,0);
		//Beep(600,10);
		//add items!
		return 0;
	  }
	  break;

	case TVN_ITEMEXPANDED:
	  if (k==200 && ((LPNMTREEVIEW)lParam)->action==TVE_COLLAPSE) {
		TreeView_Expand(TreeW,((LPNMTREEVIEW)lParam)->itemNew.hItem,TVE_COLLAPSE | TVE_COLLAPSERESET);
	  }
	  //MessageBeep(MB_OK);
	  break;

	case TVN_BEGINLABELEDIT:
	  if (k==200) {
		if (!CanKeyBeRenamed(TreeW,((LPNMTVDISPINFO)lParam)->item.hItem)) return 1;
		return 0;
	  }
	  break;
	case TVN_ENDLABELEDIT:
	  if (k==200) {
		fchar s, d; TCHAR *c;
		int n;
		if (((LPNMTVDISPINFO)lParam)->item.pszText==NULL) return 0;
		if (!CanKeyBeRenamed(TreeW,((LPNMTVDISPINFO)lParam)->item.hItem)) return 0;//Cancel
		s.c=GetKeyNameByItem(TreeW,((LPNMTVDISPINFO)lParam)->item.hItem);
		d.c=(TCHAR*)malloc((_tcslen(s.c)+_tcslen(((LPNMTVDISPINFO)lParam)->item.pszText)+10) * sizeof(TCHAR));
		_tcscpy(d.c, s.c); c = _tcsrchr(d.c, '\\');
		if (!c) {//Must never happen!
		  return 0;
		}
		_tcscpy(c+1,((LPNMTVDISPINFO)lParam)->item.pszText);
        extern bool flag_merge_chosen; flag_merge_chosen = false;
		if (!IsSubKey(s.c, d.c) || !IsSubKey(d.c, s.c)) n = MoveKey(s.c, d.c);
		else n = 0;
        if (n == 1 && !_tcscmp(s.c, currentitem)) {
		  free(currentitem);
		  currentitem = d.c; d.c = 0;
        }
        if (n == 1 && (_tcschr(c + 1, '\\') || flag_merge_chosen)) SendMessage(hwnd,WM_COMMAND,340,0);
		return n;
	  }
	  break;

	case LVN_BEGINLABELEDIT:
	  if (k==201) {
		hk=GetKeyByName(currentitem,KEY_QUERY_VALUE | KEY_SET_VALUE);
		if ((HANDLE)hk==INVALID_HANDLE_VALUE) return 1;
		RegCloseKey(hk);
		return 0;
	  }
	  break;
	case LVN_ENDLABELEDIT:
	  if (k==201) {
		NMLVDISPINFO *di=(NMLVDISPINFO*)lParam;
		if (!di->item.pszText) return 0;
		hk=GetKeyByName(currentitem,KEY_QUERY_VALUE | KEY_SET_VALUE);
		if ((HANDLE)hk==INVALID_HANDLE_VALUE) return 0;
		fchar oldname(malloc(2048)); *oldname.c = 0;
		ListView_GetItemText(ListW,di->item.iItem,0,oldname.c,2048);//!
        TCHAR *newname = di->item.pszText;
        int rv = RenameKeyValue(hk, hk, newname, oldname.c);
		RegCloseKey(hk);
		return !rv;
	  }
	  break;

	case TVN_KEYDOWN:
      wVKey = ((LPNMTVKEYDOWN)lParam)->wVKey;
	  if (wVKey == 9) {
		LastFocusedW=ListW;
		SetFocus(LastFocusedW);
		return 1;
	  }
      if (wVKey == VK_F5) {
        SendMessage(hwnd,WM_COMMAND,340,0);
		return 1;
      }
      if (wVKey == VK_F3) {
        DoSearchAndReplaceNext(hwnd);
        return 1;
      }
      if ((wVKey == 'H' || wVKey == 'F') && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        DoSearchAndReplace(hwnd, false, wVKey == 'F');
        return 1;
      }
      if (wVKey == 27 && is_dragging) ValuesEndDrag(hwnd, false);
	  if (wVKey == VK_DELETE && currentitem) {
		if (MessageBox(hwnd,_T("Are you sure you want to delete this key?"),
		  currentitem,MB_ICONQUESTION | MB_OKCANCEL)==IDOK) {
          if (!CanDeleteThisKey(currentitem, true)) return 1;
		  hk=GetKeyByName(currentitem,KEY_QUERY_VALUE | KEY_SET_VALUE | 
			KEY_ENUMERATE_SUB_KEYS | KEY_CREATE_SUB_KEY);
		  if ((HANDLE)hk==INVALID_HANDLE_VALUE) {
			MessageBox(hwnd,currentitem,_T("Access denied:"),MB_ICONWARNING);
			return 1;
		  }
		  k=DeleteAllSubkeys(hk);
		  RegCloseKey(hk);
		  if (k!=1 || DeleteKeyByName(currentitem)!=ERROR_SUCCESS) {
			MessageBox(hwnd,_T("Could not entirely delete this key"),currentitem,MB_ICONWARNING);
			SendMessage(hwnd,WM_COMMAND,340,0);
			return 1;
		  }
		  free(currentitem);
		  currentitem=NULL;
		  TreeView_DeleteItem(TreeW,currentitem_tv);
		  //SendMessage(hwnd,WM_COMMAND,340,0);??
		  return 1;
		}
	  }
	  break;
    case LVN_ITEMCHANGING: 
      if (is_dragging) return 1;
      return 0;

    case LVN_KEYDOWN:
      wVKey = ((LPNMLVKEYDOWN)lParam)->wVKey;
	  if (wVKey == 9) {
		LastFocusedW=TreeW;
		SetFocus(LastFocusedW);
		return 1;
	  }
      if (wVKey == VK_F3) {
        DoSearchAndReplaceNext(hwnd);
        return 1;
      }
      if ((wVKey == 'H' || wVKey == 'F') && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        DoSearchAndReplace(hwnd, false, wVKey == 'F');
        return 1;
      }
      if (wVKey == 27 && is_dragging) ValuesEndDrag(hwnd, false);
	  if (wVKey == VK_DELETE && currentitem) {
		k=ListView_GetSelectedCount(ListW);
		if (k && MessageBox(hwnd,_T("Are you sure you want to delete these value(s)?"),
			currentitem,MB_ICONQUESTION | MB_OKCANCEL)==IDOK) {
		  DWORD ns,ds; TCHAR *s;
		  hk=GetKeyByName(currentitem,KEY_QUERY_VALUE | KEY_SET_VALUE);
		  if ((HANDLE)hk==INVALID_HANDLE_VALUE) {
			MessageBox(hwnd,currentitem,_T("Access denied:"),MB_ICONWARNING);
			return 1;
		  }
		  if (RegQueryInfoKey(hk,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&ns,&ds,NULL,NULL)!=
			  ERROR_SUCCESS) {
			RegCloseKey(hk);
			return 1;
		  }
		  if (!k) return 1;//???
		  s=(TCHAR*)malloc((ns+=32) * sizeof(TCHAR));
		  n=-1;
		  while((n=ListView_GetNextItem(ListW,n,LVNI_SELECTED))>=0) {
			*s=0;
			ListView_GetItemText(ListW,n,0,s,ns-1);
			if (RegDeleteValue(hk,s)==ERROR_SUCCESS && ListView_DeleteItem(ListW,n)) n--,k--;
		  }
		  free(s);
		  if (k!=0) {
			MessageBox(hwnd,_T("Could not delete all specified values"),currentitem,MB_ICONWARNING);
			RegCloseKey(hk);
			return 1;
		  }
		  //SendMessage(hwnd,WM_COMMAND,340,0);???
		  RegCloseKey(hk);
		  return 1;
		}
		return 1;
	  }
	  break;
	case NM_RETURN:
	  if (k!=201) break;
	  if (!ListView_GetSelectedCount(ListW)) break;
	  i = ListView_GetNextItem(ListW, -1, LVNI_FOCUSED);
	  goto e201dbl;
	case NM_DBLCLK:
	  if (k==201) {
		i=((LPNMLISTVIEW)lParam)->iItem;
e201dbl:
		if (i<0) break;
		DWORD ns=0;
		val_ed_dialog_data dp;
        dp.keyname = currentitem;
        GetLVItemText(ListW, i, dp.name.c, ns);
        if (dp.EditValue(hwnd)) {
          break;
        }
        fchar vdp(malloc(max((int)dp.newdata.l * 3, 32)));
		
        LVITEM item;
        item.iItem=i, item.stateMask=0;
        item.mask=LVIF_TEXT, item.iSubItem=1;
        item.pszText=vdp.c;
        GetValueDataString((char*)dp.newdata.c, vdp.c, dp.newdata.l, dp.type);
        ListView_SetItem(ListW,&item);
	  }
	  break;

	case NM_RCLICK:
	  mp = GetMessagePos();
	  POINTSTOPOINT(pt, mp);
	  if (k==200) {
		TVHITTESTINFO hti;
		hti.pt = pt;
		ScreenToClient(TreeW, &hti.pt);
		TreeView_HitTest(TreeW, &hti);
		if (hti.flags & TVHT_ONITEM) {
		  TreeView_SelectItem(TreeW, hti.hItem);
		  pum = CreatePopupMenu();
		  AddSelCommandsToMenu(TreeW, pum);
		  TrackPopupMenu(pum, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
		  DestroyMenu(pum);
		}
	  } else if (k==201) {
		if (currentitem) {
		  pum = CreatePopupMenu();
		  AddSelCommandsToMenu(ListW, pum);
		  TrackPopupMenu(pum, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
		  DestroyMenu(pum);
		}
	  }
	  return 0;
	  break;

	case NM_SETFOCUS:
	  if (k==200) LastFocusedW=TreeW;
	  else if (k==201) LastFocusedW=ListW;
	  break;

    case LVN_BEGINDRAG:
      void ValuesBeginDrag(HWND hwnd, LPARAM lParam);
      ValuesBeginDrag(hwnd, lParam);
      break;

    case TVN_BEGINDRAG:
      void KeyBeginDrag(HWND hwnd, LPARAM lParam);
      KeyBeginDrag(hwnd, lParam);
      break;

	default: return 0;
    } // end of WM_NOTIFY
	break;

  case WM_LBUTTONDOWN:
    if (wParam==MK_LBUTTON && LOWORD(lParam)>=xTree && LOWORD(lParam)<xTree+xSplitBar) {
      onWpos=true,xWpos=LOWORD(lParam)-xTree;
      SetCapture(hwnd);
    }
    break;

  case WM_RBUTTONDOWN:
    if (is_dragging) ValuesEndDrag(hwnd, false);
    break;
    
  case WM_LBUTTONUP:
    if (onWpos) {
      onWpos=false;
      ReleaseCapture();
      if (LOWORD(lParam)-xWpos>5 && LOWORD(lParam)-xWpos+xSplitBar<dxw-5) {
        xTree=LOWORD(lParam)-xWpos;
        SetWindowPos(TreeW,HWND_TOP,0,CbarHeight,min(dxw,xTree),dyw-SbarHeight-CbarHeight,0);
        SetWindowPos(ListW,HWND_TOP,xTree+xSplitBar,CbarHeight,dxw-xTree-xSplitBar,dyw-SbarHeight-CbarHeight,0);
      }
    }
    if (is_dragging) ValuesEndDrag(hwnd, true);
    break;

  case WM_MOUSEMOVE:
    {
      MSG ttmsg;
      ttmsg.hwnd = hwnd;
      ttmsg.message = msg;
      ttmsg.wParam = wParam;
      ttmsg.lParam = lParam;
      GetCursorPos(&ttmsg.pt);
      ttmsg.time = GetMessageTime();
      SendMessage(hwndToolTip, TTM_RELAYEVENT, 0, (LPARAM)&ttmsg);
    }
    if (onWpos && LOWORD(lParam)-xWpos>5 && LOWORD(lParam)-xWpos+xSplitBar<dxw-5) {
      xTree=LOWORD(lParam)-xWpos;
      SetWindowPos(TreeW,HWND_TOP,0,CbarHeight,min(dxw,xTree),dyw-SbarHeight-CbarHeight,0);
      SetWindowPos(ListW,HWND_TOP,xTree+xSplitBar,CbarHeight,dxw-xTree-xSplitBar,dyw-SbarHeight-CbarHeight,0);
    }
    if (is_dragging) {
      ValuesContinueDrag(hwnd, lParam);
    }
    break;

  case WM_SIZE:
    SendMessage(SbarW,WM_SIZE,wParam,lParam);
    dxw=LOWORD(lParam),dyw=HIWORD(lParam);
    SetWindowPos(TreeW,HWND_TOP,0,CbarHeight,min(dxw,xTree),dyw-SbarHeight-CbarHeight,0);
    SetWindowPos(ListW,HWND_TOP,xTree+xSplitBar,CbarHeight,dxw-xTree-xSplitBar,dyw-SbarHeight-CbarHeight,0);
    //if (wParam==SIZE_MINIMIZED) ShowWindow(hwnd,SW_HIDE);
    break;
    
  case WM_SETFOCUS:
    SetFocus(LastFocusedW);
    break;

  case WM_CAPTURECHANGED:
    if (onWpos) onWpos = false;
    if (is_dragging) ValuesEndDrag(hwnd, false);
    break;

  default:
    return DefWindowProc (hwnd,msg,wParam,lParam);

  }
  return 0;
}

typedef set<TCHAR*, str_less_than> mystrhash;

int RefreshSubtree(HTREEITEM hti, const TCHAR *kname, HKEY hk) {
  TCHAR buf[4096];//tmp?
  int n;
  int knl = _tcslen(kname);
  TVITEM tvi, tvi1;
  HTREEITEM hfc = TreeView_GetChild(TreeW, hti), hfcold;
  mystrhash had_keys;
  for(n = 0; hfc; n++) {
    HKEY sk;
    tvi.mask=TVIF_HANDLE | TVIF_TEXT | TVIF_STATE;
    tvi.hItem=hfc;
    tvi.stateMask = TVIS_EXPANDED;
    tvi.pszText = buf, tvi.cchTextMax = 4095;
    if (!TreeView_GetItem(TreeW, &tvi)) {ErrMsgDlgBox(_T("RefreshSubtree")); return 1;}
    bool is_exp = !!(tvi.state & TVIS_EXPANDED);
    TCHAR *curname = (TCHAR*)malloc((knl + _tcslen(buf) + 2) * sizeof(TCHAR));
    _tcscpy(curname, kname); curname[knl] = '\\'; _tcscpy(curname + knl + 1, buf);

    bool is_deleted = false;
    if (RegOpenKeyEx(hk, buf, 0, KEY_ENUMERATE_SUB_KEYS | KEY_EXECUTE, &sk) == ERROR_SUCCESS) {
      DWORD has_subkeys;
      if (is_exp) 
        has_subkeys = RefreshSubtree(hfc, curname, sk);
      else {
        if (RegQueryInfoKey(sk, 0, 0, 0, &has_subkeys, 0, 0, 0, 0, 0, 0, 0))
          has_subkeys = (DWORD)-1;
      }
      RegCloseKey(sk);
      if (has_subkeys != (DWORD)-1) {
        tvi1.mask = TVIF_HANDLE | TVIF_CHILDREN;
        tvi1.hItem = hfc, tvi1.cChildren = has_subkeys != 0;
        TreeView_SetItem(TreeW, &tvi1);
      }
      if (had_keys.find(buf) == had_keys.end()) {
        had_keys.insert(_tcsdup(buf));
      }
    } else {
        if (RegOpenKeyEx(hk, buf, 0, 0, &sk) == ERROR_SUCCESS) { //??
            RegCloseKey(sk);
        } else {
            is_deleted = true;
        }
    }
    free(curname);
    if (is_deleted) hfcold = hfc;
    hfc = TreeView_GetNextSibling(TreeW, hfc);
    if (is_deleted) TreeView_DeleteItem(TreeW, hfcold);
  }
  DWORD count_subkeys = had_keys.size();
  DWORD sns = MAX_PATH + 1;
  TCHAR *sn = (TCHAR*)malloc(sns * sizeof(TCHAR));
  FILETIME ft;
  TVINSERTSTRUCT tvins;
  for(n = 0;; n++) {
    sns = MAX_PATH + 1;
    LONG rv = RegEnumKeyEx(hk, n, sn, &sns, 0, 0,0, &ft);
    if (rv != 0) break;
    if (n == 0) {
      tvins.hParent = hti, tvins.hInsertAfter = TVI_SORT;
      tvins.item.mask =  TVIF_TEXT | TVIF_CHILDREN | TVIF_STATE;
      tvins.item.state=0, tvins.item.stateMask=TVIS_EXPANDEDONCE;
      tvins.item.pszText = sn;
    }
    if (had_keys.find(sn) == had_keys.end()) {
      HKEY sk;
      DWORD has_subkeys = 0;
      if (!RegOpenKeyEx(hk, sn, 0, KEY_EXECUTE, &sk)) {
        RegQueryInfoKey(sk, 0, 0, 0, &has_subkeys, 0, 0, 0, 0, 0, 0, 0);
        RegCloseKey(sk);
      }
      tvins.item.cChildren = has_subkeys != 0;
      TreeView_InsertItem(TreeW, &tvins);
      count_subkeys++;
    }
  }
  free(sn);
  mystrhash::iterator i = had_keys.begin();
  while(i != had_keys.end()) {
    TCHAR *c = (*i);
    i++;
    delete []c;
  }
  had_keys.clear();
  return count_subkeys;
}

const TCHAR* TypeCName(DWORD type) {
  switch (type) {
  case REG_SZ:return _T("REG_SZ");
  case REG_BINARY:return _T("REG_BINARY");
  case REG_DWORD:return _T("REG_DWORD");
  case REG_MULTI_SZ:return _T("REG_MULTI_SZ");
  case REG_DWORD_BIG_ENDIAN:return _T("REG_DWORD_BIG_ENDIAN");
  case REG_EXPAND_SZ:return _T("REG_EXPAND_SZ");
  case REG_RESOURCE_LIST:return _T("REG_RESOURCE_LIST");
  case REG_FULL_RESOURCE_DESCRIPTOR:return _T("REG_FULL_RESOURCE_DESCRIPTOR");
  case REG_RESOURCE_REQUIREMENTS_LIST:return _T("REG_RESOURCE_REQUIREMENTS_LIST");
  case REG_LINK:return _T("REG_LINK");
  case REG_NONE:return _T("REG_NONE");
  default:return _T("");
  }
}

INT_PTR CALLBACK ValTypeDlg (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  static int edval,cval,lock;
  TCHAR s[64];
  int n;
  switch (msg) {
  case WM_INITDIALOG: 
	cval=edval=lParam;
	lock=1;
	_stprintf(s,_T("%i"),(int)lParam);
	SetDlgItemText(hwnd,IDC_DECNUM,s);
	_stprintf(s,_T("%X"),(int)lParam);
	SetDlgItemText(hwnd,IDC_HEXNUM,s);
	_stprintf(s,_T("0x%08X (%i) \"%s\""),(int)lParam,(int)lParam,TypeCName(lParam));
	SetDlgItemText(hwnd,IDC_TYPEID,s);
	SendDlgItemMessage(hwnd,IDC_DECNUM,EM_SETLIMITTEXT,32,0);
	SendDlgItemMessage(hwnd,IDC_HEXNUM,EM_SETLIMITTEXT,32,0);
	lock=0;
	return 0;
  case WM_CLOSE: EndDialog(hwnd,edval); return 1;
  case WM_COMMAND:
	switch (LOWORD (wParam)) {
	case IDOK: 
	  EndDialog(hwnd,cval);
	  break;
	case IDCANCEL: EndDialog(hwnd,edval); break; 

	case IDC_DECNUM:
	  if (HIWORD(wParam)==EN_UPDATE && !lock) {
		GetDlgItemText(hwnd,IDC_DECNUM,s,64);
		_stscanf(s,_T("%i"),&cval);
		_stprintf(s,_T("%X"),cval);
		lock=1;
		SetDlgItemText(hwnd,IDC_HEXNUM,s);
		_stprintf(s,_T("0x%08X (%i) \"%s\""),cval,cval,TypeCName(cval));
		SetDlgItemText(hwnd,IDC_TYPEID,s);
		lock=0;
	  }
	  break;
	case IDC_HEXNUM:
	  if (HIWORD(wParam)==EN_UPDATE && !lock) {
		GetDlgItemText(hwnd,IDC_HEXNUM,s,64);
		for(n=0,cval=0; isxdigit((unsigned char)s[n]); n++) 
          cval = (cval << 4) | (s[n] <= '9' ? s[n]-'0' : _totupper(s[n]) - 'A' + 10);
		_stprintf(s,_T("%i"),cval);
		lock=1;
		SetDlgItemText(hwnd,IDC_DECNUM,s);
		_stprintf(s,_T("0x%08X (%i) \"%s\""),cval,cval,TypeCName(cval));
		SetDlgItemText(hwnd,IDC_TYPEID,s);
		lock=0;
	  }
	  break;
	}
	return 1;
  }
  return 0;
}

void LoadSettings() {
	DWORD dw;
	int n;
	//TCHAR s[20];
	HKEY MainKey;

	Settings[0]=0x2C, Settings[1]=0, Settings[2]=1; 
	Settings[3]=Settings[4]=Settings[5]=Settings[6]=-1;
	Settings[7]=Settings[8]=10; Settings[9]=600; Settings[10]=400;
	Settings[11]=Settings[12]=150, Settings[13]=400, Settings[14]=1;
	RegCreateKeyEx(HKEY_CURRENT_USER,
	  _T("Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit"),0,_T("RE"),
	  REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,&MainKey,&dw);
	if (dw==REG_CREATED_NEW_KEY) return;
	dw=64;
	RegQueryValueEx(MainKey,_T("View"),0,NULL,(LPBYTE)&Settings,&dw);
	dw=4;
	if (RegQueryValueEx(MainKey,_T("FindFlags"),0,NULL,(LPBYTE)&n,&dw)!=ERROR_SUCCESS) n=0;
	
	sMatch=n&1, sKeys=(n>>1)&1, sVal=(n>>2)&1, sDat=(n>>3)&1;
	//xw=Settings[7], yw=Settings[8], dxw=Settings[9]-xw, dyw=Settings[10]-yw;
	//xTree=Settings[11], xName=Settings[12], xData=Settings[13];

	RegCloseKey(MainKey);
}

HMENU TypeMenu(DWORD p,int key,DWORD disable) {
  HMENU mn1=CreatePopupMenu();
  if (!key) {
	AppendMenu(mn1,MF_STRING,p+0,_T("&Key"));
	AppendMenu(mn1,MF_SEPARATOR,-1,_T(""));
  }
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==1),p+1,_T("&String Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==2),p+2,_T("&Binary Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==3),p+3,_T("&DWORD Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==4),p+4,_T("&Multistring Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==5),p+5,_T("Big Endian D&WORD Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==6),p+6,_T("&Env. String Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==7),p+7,_T("&Resource List Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==8),p+8,_T("&Full Resource Descriptor Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==9),p+9,_T("Res. Re&quirements List Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==10),p+10,_T("Sym&Link Value"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==11),p+11,_T("&Value with no type"));
  AppendMenu(mn1,MF_STRING | disable | MF_CHECKED*(key==12),p+12,_T("&Custom value type..."));
  return mn1;
}

int GetTypeMnuNo(DWORD type) {
  switch (type) {
  case REG_SZ:return 1;
  case REG_BINARY:return 2;
  case REG_DWORD:return 3;
  case REG_MULTI_SZ:return 4;
  case REG_DWORD_BIG_ENDIAN:return 5;
  case REG_EXPAND_SZ:return 6;
  case REG_RESOURCE_LIST:return 7;
  case REG_FULL_RESOURCE_DESCRIPTOR:return 8;
  case REG_RESOURCE_REQUIREMENTS_LIST:return 9;
  case REG_LINK:return 10;
  case REG_NONE:return 11;
  default:return 12;
  }
}

DWORD GetRegValueType(DWORD k,DWORD type) {
  switch(k) {
  case 1:return REG_SZ;
  case 2:return REG_BINARY;
  case 3:return REG_DWORD;
  case 4:return REG_MULTI_SZ;
  case 5:return REG_DWORD_BIG_ENDIAN;
  case 6:return REG_EXPAND_SZ;
  case 7:return REG_RESOURCE_LIST;
  case 8:return REG_FULL_RESOURCE_DESCRIPTOR;
  case 9:return REG_RESOURCE_REQUIREMENTS_LIST;
  case 10:return REG_LINK;
  case 11:return REG_NONE;
  default:return DialogBoxParam(hInst,_T("TYPENO"),MainWindow,ValTypeDlg,(LPARAM)type);
  }
}
DWORD GetMinRegValueSize(DWORD v) {
  switch (v) {
  case REG_SZ:return 1;//!?
  case REG_BINARY:return 0;
  case REG_DWORD:return 4;
  case REG_MULTI_SZ:return 2;//!?
  case REG_DWORD_BIG_ENDIAN:return 4;
  case REG_EXPAND_SZ:return 1;
  case REG_RESOURCE_LIST:return 4;//!?
  case REG_FULL_RESOURCE_DESCRIPTOR:return 4;//!?
  case REG_RESOURCE_REQUIREMENTS_LIST:return 4;//!?
  case REG_LINK:return 1;//!?
  case REG_NONE:return 0;
  default:return 0;
  }
}

int ValueTypeIcon(DWORD type) {
  switch(type) {
  case REG_SZ:return 0;
  case REG_BINARY:case REG_DWORD:return 1;
  case REG_DWORD_BIG_ENDIAN:return 2;
  case REG_EXPAND_SZ:return 3;
  case REG_LINK:return 4;
  case REG_MULTI_SZ:return 5;
  case REG_NONE:default:return 6;
  case REG_FULL_RESOURCE_DESCRIPTOR:
  case REG_RESOURCE_REQUIREMENTS_LIST:
  case REG_RESOURCE_LIST:return 7;
  }
}

void GetLVItemText(HWND ListW, int i, TCHAR *&name, DWORD &ns) {
  if (!ns) {
    ns = 128;
    name = (TCHAR*)realloc(name, ns * sizeof(TCHAR));
  }
  LVITEM lvi;
  lvi.mask = LVIF_TEXT, lvi.iSubItem = 0;
  while(1) {
    lvi.pszText = name, lvi.cchTextMax = ns;
    DWORD rv = SendMessage(ListW, LVM_GETITEMTEXT, (WPARAM)i, (LPARAM)&lvi);
    if (rv < ns - 1) break;
    ns *= 2;
    name = (TCHAR*)realloc(name, ns * sizeof(TCHAR));
  }
}


void GetValueDataString(char *val, TCHAR *valb,int m1,DWORD type) {
  int k,l; BYTE b; TCHAR *valpos;
  switch(type) {
  case REG_SZ:case REG_EXPAND_SZ:
    m1 /= sizeof(TCHAR);
    if (!PTSTR(val)[m1-1]) m1--;
    valb[0]='\"';
    for(k=0;k<m1;k++) if (PTSTR(val)[k]==0) valb[k+1]=9; else valb[k+1]=PTSTR(val)[k];
    valb[k+1]='\"', valb[k+2]=0;
    break;
  case REG_MULTI_SZ:
    m1 /= sizeof(TCHAR);
    if (!PTSTR(val)[m1-1]) m1--;
    l=0;
    valb[l++]='\"';
    for(k=0;k<m1;k++) if (PTSTR(val)[k]==0) {
      if (k+1<m1) valb[l++]='\"',valb[l++]=9,valb[l++]='\"';
    } else valb[l++]=PTSTR(val)[k];
    valb[l++]='\"',valb[l++]=0;
    break;
  case REG_DWORD_BIG_ENDIAN:
    b=val[0],val[0]=val[3],val[3]=b;
    b=val[1],val[1]=val[2],val[2]=b;//dirty
  case REG_DWORD:
    k=*(DWORD*)val;
    if (m1<4) k&=~(-1<<m1*8);
    _stprintf(valb,_T("0x%08X (%i)"),k,k);
    break;
  default:
    if (!m1) _tcscpy(valb,_T("(No bytes)"));
    else {
      for(k=0,valpos=valb;k<m1-1;k++,valpos+=3) 
        _stprintf(valpos,_T("%02X "),(BYTE)val[k]);
      _stprintf(valpos,_T("%02X"),(BYTE)val[k]);
    }
    break;
  }
}

int ListValues(HWND hwnd,TCHAR *s) {
  LVITEM item;
  HKEY hk;
  DWORD num_ok = 0;
  ListView_DeleteAllItems(ListW);
  item.mask=LVIF_TEXT | LVIF_IMAGE /*| LVIF_PARAM*/, item.iItem=0, item.iSubItem=0;
  item.stateMask=0, item.lParam=0;

  hk=GetKeyByName(s,KEY_QUERY_VALUE);
  LONG hk_err = hk == (HKEY)INVALID_HANDLE_VALUE? lastRegErr : 0;
  value_iterator i(hk);

  if (hk_err || i.err()) {
    item.pszText=_T("Error!");
    item.iImage=6;
    int I = ListView_InsertItem(ListW,&item);
    CloseKey_NHC(hk);
    if (hk_err && I >= 0) {
      TCHAR *errstr;
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, hk_err, 0, (TCHAR*)&errstr, 0, 0);
      item.pszText = errstr;
      item.iItem=I, item.iSubItem=1;
      ListView_SetItem(ListW, &item);
      item.iSubItem=0;
      LocalFree(errstr);
    }
	return 1;
  }
  achar valb(max<int>(i.data.s*3, 31) * sizeof(TCHAR));
  item.pszText=i.name.c;
  for (; !i.end(); i++) 
	if (i.is_ok) {
	  num_ok++;
	  item.iImage = ValueTypeIcon(i.type);
	  int I = ListView_InsertItem(ListW,&item);
	  if (I >= 0) {
		GetValueDataString((char*)i.data.c, (TCHAR*)valb.c, i.data.l, i.type);
		item.pszText=(TCHAR*)valb.c;
		item.iItem=I;
		item.iSubItem=1;
		ListView_SetItem(ListW,&item);
		item.iSubItem=0;
	  }
	  item.pszText = i.name.c;
	}
	/*{
		LVFINDINFO finf;
		finf.flags=LVFI_STRING, finf.psz="";
		n=ListView_FindItem(ListW,-1,&finf);
		if (n>=0) {
		  item.iItem=n;
		  item.pszText="(Default)", item.lParam=1;
		  item.mask=LVIF_TEXT | LVIF_PARAM, item.iSubItem=0;
		  ListView_SetItem(ListW,&item);
		}
	}*/
	if (i.num_val != num_ok) {
	  TCHAR *sss = (TCHAR*)alloca((100 + _tcslen(s)) * sizeof(TCHAR));
	  _stprintf(sss,_T("Could not retrieve all values for %s: me=%u != mv=%u, mln=%u, mld=%u, m0=%i, m1=%i\nPlease retry!"),s,num_ok,i.num_val,i.name.s,i.data.s,i.name.l,i.data.l);
      SendMessage(SbarW,SB_SETTEXT,0,(LPARAM)sss);
	}
	CloseKey_NHC(hk);
	//Beep(400,10);
	return 0;
}

void ValuesBeginDrag(HWND hwnd, LPARAM lParam) {
  NMLISTVIEW &lvnm = *(NMLISTVIEW*)lParam;
  dragitno = lvnm.iItem;
  dragiml = ImageList_Create(1, 1, ILC_COLOR, 0, 1);
  //try to assure that dragged items always look the same way - don't call only
  // ListView_CreateDragImage(ListW, dragitno, &lvnm.ptAction); for the first item
  int n = -1;
  int firstx = -1, firsty = -1;
  POINT pt; RECT rct;
  while((n=ListView_GetNextItem(ListW,n,LVNI_SELECTED)) >= 0) {
    HIMAGELIST il2 = ListView_CreateDragImage(ListW, n, &pt);
    if (!il2) { ErrMsgDlgBox(_T("il2")); continue; }
    if (firsty == -1) firstx = pt.x, firsty = pt.y;
    HIMAGELIST ml = ImageList_Merge(dragiml, 0, il2, 0, 0, pt.y - firsty);
    ImageList_Destroy(dragiml); ImageList_Destroy(il2);
    dragiml = ml;
  }
  
  // DragMove didn't display the result of ImageList_Merge, so let's fix that
  HIMAGELIST l = dragiml;
  int x = -2, y = -2;
  ImageList_GetIconSize(l, &x, &y);
  HDC hdc = GetDC(hwnd); 
  HDC dc1 = CreateCompatibleDC(hdc);
  HBITMAP bm1 = CreateCompatibleBitmap(hdc, x, y);
  if (!SelectObject(dc1, bm1)) ErrMsgDlgBox(_T("SelectObject"));
  ImageList_DrawEx(l, 0, dc1, 0, 0, 0, 0, CLR_NONE, CLR_NONE, ILD_IMAGE);
  //BitBlt(hdc, 100, 200, x, y, dc1, 0, 0, SRCCOPY); //for debugging
  DeleteDC(dc1);
  ImageList_RemoveAll(l);
  ImageList_AddMasked(l, bm1, 0);
  //ImageList_DrawEx(l, 1, hdc, 100, 260, 0, 0, CLR_NONE, CLR_NONE, ILD_IMAGE); //for debugging
  ReleaseDC(hwnd, hdc);
  DeleteObject(bm1);
  
  pt = lvnm.ptAction;
  if (!ImageList_BeginDrag(dragiml, 0, pt.x - firstx, pt.y - firsty)) ErrMsgDlgBox(_T("ImageList_BeginDrag (dragiml)"));/*error handling*/;
  ClientToScreen(ListW, &pt);
  GetWindowRect(hwnd, &rct);
  if (!ImageList_DragEnter(hwnd, pt.x - rct.left, pt.y - rct.top)) ErrMsgDlgBox(_T("ImageList_DragEnter"));
  is_dragging = true;
  SetCapture(hwnd); 
  SetCursor(curs_no); 
  prevdhlti = 0; prev_candrop = false, could_ever_drop = false;
  prevdhltibtm = GetTickCount();
  is_key_dragging = false;
  //EnableWindow(ListW, false); SetFocus(LastFocusedW = TreeW);
}

void KeyBeginDrag(HWND hwnd, LPARAM lParam) {
  NMTREEVIEW &tvnm = *(NMTREEVIEW*)lParam;
  //dragiml = ImageList_Create(16, 16, ILC_COLOR, 0, 1);
  //ImageList_AddIcon(dragiml, regsmallicon);
  dragiml = ImageList_LoadBitmap(hInst, _T("BMPKEYDRAG"), 16, 0, 0x00FFFFFF);

  TCHAR *s = GetKeyNameByItem(TreeW, currentitem_tv = tvnm.itemNew.hItem);
  if (currentitem) free(currentitem);
  currentitem = s;
  TreeView_SelectItem(TreeW, currentitem_tv);

  POINT pt = tvnm.ptDrag; RECT rct;
  TreeView_GetItemRect(TreeW, tvnm.itemNew.hItem, &rct, true);
  if (!ImageList_BeginDrag(dragiml, 0, /*pt.x - rct.left*/25, pt.y - rct.top)) ErrMsgDlgBox(_T("ImageList_BeginDrag (dragiml)"));/*error handling*/;
  ClientToScreen(TreeW, &pt);
  GetWindowRect(hwnd, &rct);
  if (!ImageList_DragEnter(hwnd, pt.x - rct.left, pt.y - rct.top)) ErrMsgDlgBox(_T("ImageList_DragEnter"));
  is_dragging = true;
  SetCapture(hwnd); 
  SetCursor(curs_no); 
  prevdhlti = 0; prev_candrop = false, could_ever_drop = false;
  prevdhltibtm = GetTickCount();
  is_key_dragging = true;
  //EnableWindow(ListW, false); SetFocus(LastFocusedW = TreeW);
}


void ValuesEndDrag(HWND hwnd, bool is_ok) {
  is_dragging = false;
  ImageList_EndDrag();
  ImageList_DragLeave(hwnd);
  
  ReleaseCapture();
  //EnableWindow(ListW, true); SetFocus(LastFocusedW = ListW);
  ImageList_Destroy(dragiml);
  SetCursor(curs_arr);
  if (prevdhlti) TreeView_SetItemState(TreeW, prevdhlti, 0, TVIS_DROPHILITED/*mask*/);
  is_ok = is_ok && prev_candrop && prevdhlti;
  if (is_ok) {
	fchar s(GetKeyNameByItem(TreeW, prevdhlti));
    if (is_key_dragging) {
      TCHAR *l = _tcsrchr(currentitem, '\\'), *m; int sl = _tcslen(s.c);
      l = l? l + 1 : currentitem;
      s.c = (TCHAR*)realloc(s.c, (sl + _tcslen(l) + 2) * sizeof(TCHAR));
      *(m = s.c + sl) = '\\';
      _tcscpy(m + 1, l);
      KeyAskMoveOrCopy(hwnd, 4, s.c);
    } else ValuesAskMoveOrCopy(hwnd, 6, s.c);
  }
  prevdhlti = 0;
}

void ValuesContinueDrag(HWND hwnd, LPARAM lParam) {
  POINT pt = { LOWORD(lParam), HIWORD(lParam) }; RECT rct;
  ClientToScreen(hwnd, &pt);
  GetWindowRect(hwnd, &rct);
  TVHITTESTINFO tvht; memset(&tvht, 0, sizeof(tvht));
  tvht.pt = pt;
  ScreenToClient(TreeW, &tvht.pt);
  HTREEITEM hti = 0;
  if (pt.x > 30000) tvht.flags = TVHT_TOLEFT; // fix wrong sign expansion
  else if (pt.y > 30000) tvht.flags = TVHT_ABOVE; // the same
  else hti = TreeView_HitTest(TreeW, &tvht);
  bool candrop = hti
    && (tvht.flags == TVHT_ONITEM || tvht.flags == TVHT_ONITEMBUTTON || tvht.flags == TVHT_ONITEMINDENT || tvht.flags == TVHT_ONITEMLABEL);
  if (!candrop) hti = 0;
  if (hti == currentitem_tv) candrop = false;
  if (candrop && !prev_candrop) {
    SetCursor(curs_arr);
    could_ever_drop = true;
  } else if (!candrop && prev_candrop) SetCursor(curs_no);
  prev_candrop = candrop;
  bool needrefresh = false;
  
  if (!candrop && could_ever_drop && (tvht.flags == TVHT_ABOVE || tvht.flags == TVHT_BELOW || tvht.flags == TVHT_TOLEFT || tvht.flags == TVHT_TORIGHT)) {
    bool rf = true;
    ImageList_DragShowNolock(false);
    switch(tvht.flags) {
    case TVHT_ABOVE: rf = SendMessage(TreeW, WM_VSCROLL, SB_LINEUP, 0) != 0; break;
    case TVHT_BELOW: rf = SendMessage(TreeW, WM_VSCROLL, SB_LINEDOWN, 0) != 0; break;
    case TVHT_TOLEFT: rf = SendMessage(TreeW, WM_HSCROLL, SB_LINELEFT, 0) != 0; break;
    case TVHT_TORIGHT: rf = SendMessage(TreeW, WM_HSCROLL, SB_LINERIGHT, 0) != 0; break;
    }
    if (rf) needrefresh = true;
    else ImageList_DragShowNolock(true);
  }
  if (prevdhlti != hti) {
    ImageList_DragShowNolock(false);
    needrefresh = true;
    if (prevdhlti) TreeView_SetItemState(TreeW, prevdhlti, 0, TVIS_DROPHILITED/*mask*/);
    if (hti) TreeView_SetItemState(TreeW, hti, TVIS_DROPHILITED, TVIS_DROPHILITED/*mask*/);
    prevdhlti = hti;
    prevdhltibtm = GetTickCount();
  } else if (hti && tvht.flags == TVHT_ONITEMBUTTON) {
    if (GetTickCount() - prevdhltibtm > 1000) {
      //expand item!
      TVITEM item;
      item.hItem = hti, item.mask = TVIF_STATE, item.stateMask = TVIS_EXPANDED | TVIS_EXPANDEDONCE;
      TreeView_GetItem(TreeW, &item);
      if (!(item.state & TVIS_EXPANDED)) {
        ImageList_DragShowNolock(false);
        needrefresh = true;
		item.mask = TVIF_CHILDREN | TVIF_STATE;
		item.cChildren = 1;
		item.state=0, item.stateMask = TVIS_EXPANDEDONCE;
		TreeView_SetItem(TreeW, &item);
        TreeView_Expand(TreeW, hti, TVE_EXPAND);
      }
    }
  }
  if (needrefresh) {
    MSG msg; //ops...
    while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
      if (msg.message == WM_QUIT) { 
        PostQuitMessage(msg.wParam);
        break; 
      }
      TranslateMessage (&msg);
      DispatchMessage(&msg);
    }
    ImageList_DragShowNolock(true);
  }
  
  if (!ImageList_DragMove(pt.x - rct.left, pt.y - rct.top)) 
    ErrMsgDlgBox(_T("ImageList_DragMove"));/*error handling*/;
}

void KeyAskMoveOrCopy(HWND hwnd, int mvflag, const TCHAR *dst) {
  TreeView_EnsureVisible(TreeW, currentitem_tv);
  free(mvcpkeyfrom); free(mvcpkeyto);
  mvcp_move = mvflag;
  mvcpkeyfrom = _tcsdup(currentitem);
  mvcpkeyto = dst? _tcsdup(dst) : 0;
  if (DialogBox(hInst,_T("MOVEKEY"),hwnd,DialogMVCP)) {
    if (mvcp_move & 1) MoveKey(mvcpkeyfrom,mvcpkeyto);
    else CopyKey(mvcpkeyfrom,mvcpkeyto,_T("copy"));
    SendMessage(hwnd,WM_COMMAND,340,0);
  }
}

void ValuesAskMoveOrCopy(HWND hwnd, int mvflag, const TCHAR *dst) {
  int k = ListView_GetSelectedCount(ListW);
  if (!(currentitem && k)) return;
  TreeView_EnsureVisible(TreeW,currentitem_tv);
  free(mvcpkeyfrom); free(mvcpkeyto);
  mvcp_move = mvflag;
  mvcpkeyfrom = _tcsdup(currentitem);
  mvcpkeyto = dst? _tcsdup(dst) : 0;
  if (DialogBox(hInst,_T("MOVEKEY"),hwnd,DialogMVCP)) {
    if (!_tcsicmp(mvcpkeyfrom, mvcpkeyto)) return;
    DWORD ns,ds; TCHAR *s;
    HKEY hk, hk2;
    hk = GetKeyByName(currentitem,KEY_QUERY_VALUE | KEY_SET_VALUE);
    if ((HANDLE)hk==INVALID_HANDLE_VALUE) {
      MessageBox(hwnd,currentitem,_T("Access denied:"),MB_ICONWARNING);
      return;
    }
    hk2= GetKeyByName(mvcpkeyto,KEY_QUERY_VALUE | KEY_SET_VALUE);
    if ((HANDLE)hk2==INVALID_HANDLE_VALUE) {
      MessageBox(hwnd,mvcpkeyto,_T("Access denied:"),MB_ICONWARNING);
      RegCloseKey(hk);
      return;
    }
    if (RegQueryInfoKey(hk,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&ns,&ds,NULL,NULL)!=0) {
      RegCloseKey(hk); RegCloseKey(hk2);
      return;
    }
    if (!k) return;//???
    s = (TCHAR*)malloc((ns+=32) * sizeof(TCHAR));
    int n = -1;
    while((n=ListView_GetNextItem(ListW,n,LVNI_SELECTED))>=0) {
      *s=0;
      ListView_GetItemText(ListW,n,0,s,ns-1);
      if (RenameKeyValue(hk, hk2, s, s, !(mvcp_move & 1)))
        continue;
      if((mvcp_move & 1) && !ListView_DeleteItem(ListW,n))
        continue;
      n--,k--;
    }
    free(s);
    if (k!=0) {
      MessageBox(hwnd, mvcp_move & 1?
        _T("Could not move all specified values") : _T("Could not copy all specified values"),
        currentitem,MB_ICONWARNING);
    }
    RegCloseKey(hk); RegCloseKey(hk2);
  }
}

INT_PTR CALLBACK DialogAbout (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    switch (msg) {
	case WM_INITDIALOG: SetDlgItemText(hwnd,110,_T("http://melkov.narod.ru/misc/tools/regedt33/")); return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDOK: EndDialog(hwnd,1);	break;
		case IDCANCEL: EndDialog(hwnd,0); break; }
		return 1;
	}
	return 0;
}

extern bool allow_delete_important_keys;
bool enable_regreplacekey_menu = false;
INT_PTR CALLBACK DialogSettings(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  switch (msg) {
  case WM_INITDIALOG: 
    SendDlgItemMessage(hwnd, IDC_C_ALLOWCRIT, BM_SETCHECK, allow_delete_important_keys, 0);
    SendDlgItemMessage(hwnd, IDC_C_ENRRKMENU, BM_SETCHECK, enable_regreplacekey_menu, 0);
    return 1;
  case WM_CLOSE: EndDialog(hwnd,0); return 1;
  case WM_COMMAND:
    switch (LOWORD (wParam)) {
    case IDOK: 
      allow_delete_important_keys = SendDlgItemMessage(hwnd, IDC_C_ALLOWCRIT, BM_GETCHECK, 0, 0) != 0;
      enable_regreplacekey_menu = SendDlgItemMessage(hwnd, IDC_C_ENRRKMENU, BM_GETCHECK, 0, 0) != 0;
      EnableMenuItem(theEditMenu, ID_EDIT_REPLACEKEY, enable_regreplacekey_menu? MF_ENABLED : MF_GRAYED);
      EndDialog(hwnd,1);
      break;
    case IDCANCEL: EndDialog(hwnd,0); break; }
    return 1;
  }
  return 0;
}

void FixKeyName(const TCHAR *k0, fchar &key) {
  const TCHAR *d, *s = rkeys.LongName2ShortName(k0, &d);
  if (s) {
    TCHAR *t = (TCHAR*)malloc((_tcslen(s) + _tcslen(d) + 2) * sizeof(TCHAR));
    _stprintf(t,_T("%s\\%s"), s, d);
    free(key.c);
    key.c = t;
  }
}
void GetAndFixKeyName(HWND hwnd, int ctrl, fchar &key) {
  getDlgItemText(key.c, hwnd, IDC_E_KEYNAME);
  const TCHAR *d, *s = rkeys.LongName2ShortName(key.c, &d);
  if (s) {
    TCHAR *t = (TCHAR*)malloc((_tcslen(s) + _tcslen(d) + 2) * sizeof(TCHAR));
    if (*d) _stprintf(t, _T("%s\\%s"), s, d);
    else _tcscpy(t, s);
    free(key.c);
    key.c = t;
    SetDlgItemText(hwnd, IDC_E_KEYNAME, key.c);
  }
}

INT_PTR CALLBACK DialogGotoKey(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
      fchar &key = *(fchar*)lParam;
      SetDlgEditCtrlHist(hwnd, IDC_E_KEYNAME);
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      if (key.c) SetDlgItemText(hwnd, IDC_E_KEYNAME, key.c);
      return 1;
    }
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
	  switch (LOWORD (wParam)) {
      case IDOK: {
        fchar &key = *(fchar*)GetWindowLongPtr(hwnd, DWLP_USER);
        GetAndFixKeyName(hwnd, IDC_E_KEYNAME, key);
        EndDialog(hwnd,1);
        break;
      }
	  case IDCANCEL: EndDialog(hwnd,0); break; 
      }
	  return 1;
	}
	return 0;
}

INT_PTR CALLBACK DialogAddFavKey(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
      fchar &key = *(fchar*)lParam, &title = (&key)[1], &value = (&key)[2], &comment = (&key)[3];
      SetDlgEditCtrlHist(hwnd, IDC_E_KEYNAME);
      SetDlgEditCtrlHist(hwnd, IDC_VNAME);
      SetDlgEditCtrlHist(hwnd, IDC_E_COMMENT);
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      if (key.c) SetDlgItemText(hwnd, IDC_E_KEYNAME, key.c);
      if (title.c) SetDlgItemText(hwnd, IDC_E_KEYTITLE, title.c);
      if (value.c) SetDlgItemText(hwnd, IDC_VNAME, value.c);
      return 1;
    }
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
	  switch (LOWORD (wParam)) {
      case IDOK: {
        fchar &key = *(fchar*)GetWindowLongPtr(hwnd, DWLP_USER), &title = (&key)[1], &value = (&key)[2], &comment = (&key)[3];
        GetAndFixKeyName(hwnd, IDC_E_KEYNAME, key);
        getDlgItemText(title.c, hwnd, IDC_E_KEYTITLE);
        if (SendDlgItemMessage(hwnd, IDC_C_USEVAL, BM_GETCHECK, 0, 0) != 0)
          getDlgItemText(value.c, hwnd, IDC_VNAME);
        else { free(value.c); value.c = 0; }
        if (SendDlgItemMessage(hwnd, IDC_C_USECOMM, BM_GETCHECK, 0, 0) != 0)
          getDlgItemText(comment.c, hwnd, IDC_E_COMMENT);
        else { free(comment.c); comment.c = 0; }
        EndDialog(hwnd,1);
        break;
      }
	  case IDCANCEL: EndDialog(hwnd,0); break; 
      case IDC_C_USEVAL:
        EnableWindow(GetDlgItem(hwnd, IDC_VNAME), SendDlgItemMessage(hwnd, IDC_C_USEVAL, BM_GETCHECK, 0, 0) != 0);
        break;
      case IDC_C_USECOMM:
        EnableWindow(GetDlgItem(hwnd, IDC_E_COMMENT), SendDlgItemMessage(hwnd, IDC_C_USECOMM, BM_GETCHECK, 0, 0) != 0);
        break;
      }
	  return 1;
	}
	return 0;
}


const TCHAR *mvcp_title[4] = { _T("Copy key"), _T("Move key"), _T("Copy value(s)"), _T("Move value(s)") };
INT_PTR CALLBACK DialogMVCP (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    switch (msg) {
	case WM_INITDIALOG: 
      SetWindowText(hwnd, mvcp_title[mvcp_move & 3]);
      if (mvcpkeyfrom) SetDlgItemText(hwnd, IDC_KEYFROM, mvcpkeyfrom);
      if (mvcp_move & 2) SendDlgItemMessage(hwnd, IDC_KEYFROM, EM_SETREADONLY, 1, 0);
      if (mvcpkeyto) SetDlgItemText(hwnd, IDC_KEYTO, mvcpkeyto);
      if (mvcp_move & 4) ShowWindow(GetDlgItem(hwnd, IDC_C_MOVE), SW_SHOW);
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
	  switch (LOWORD (wParam)) {
	  case IDOK: 
        getDlgItemText(mvcpkeyfrom, hwnd, IDC_KEYFROM);
        getDlgItemText(mvcpkeyto, hwnd, IDC_KEYTO);
        EndDialog(hwnd,1);
        break;
	  case IDCANCEL: EndDialog(hwnd,0); RplProgrDlg = 0; break; 
      }
      case IDC_C_MOVE:
        mvcp_move = (mvcp_move & ~1) | (SendDlgItemMessage(hwnd, IDC_C_MOVE, BM_GETCHECK, 0,0) != 0);
        SetWindowText(hwnd, mvcp_title[mvcp_move & 3]);
        break;
	  return 1;
	}
	return 0;
}

void getDlgItemText(TCHAR *&var, HWND hwnd, int ctrl) {
  if (var) free(var), var = 0;
  int len = SendDlgItemMessage(hwnd,ctrl,WM_GETTEXTLENGTH,0,0);
  if (len < 0) return;
  var = (TCHAR*)malloc((len + 2) * sizeof(TCHAR));
  GetDlgItemText(hwnd, ctrl, var, len + 1);
}

UINT achar::GetDlgItemText(HWND hwnd, int nIDDlgItem) { //single-threaded!
  DWORD n = SendDlgItemMessage(hwnd, nIDDlgItem, WM_GETTEXTLENGTH, 0, 0);
  resize(n * sizeof(TCHAR)); //?
  return l = ::GetDlgItemText(hwnd, nIDDlgItem, (TCHAR *)c, s) * sizeof(TCHAR);
}

LONG achar::QueryValue(HKEY hk, const TCHAR *name, DWORD &type) {
  l = s;
  long rv = RegQueryValueEx(hk, name, 0, &type, (BYTE*)c, &l);
  if ((rv || !c) && l >= s) {
    resize(); l = s;
    rv = RegQueryValueEx(hk, name, 0, &type, (BYTE*)c, &l);
  }
  return rv;
}

// a kind of ... something approximate
// note that input string is currently supposed to be 0-protected (not necessarily 0-terminated)
int ProcessCEscapes(TCHAR *c, int l) {
  if (!c) return 0;
  TCHAR *e = c + l, *d = c, *cc = c;
  for (; c < e; c++) {
    if (*c != '\\') *d++ = *c;
    else switch (c[1]) {
      case '\\': *d++ = '\\', c++; break;
      case 'r': *d++ = '\r', c++; break;
      case 'n': *d++ = '\n', c++; break;
      case 't': *d++ = '\t', c++; break;
      case 'x': case 'X':
        { // hexadecimal; no more than two symbols
          c++;
          int cval = 0;
          if (isxdigit((unsigned char)c[1])) {
            c++;
            cval = *c <= '9' ? *c - '0' : _totupper(*c) - 'A' + 10;
            if (isxdigit((unsigned char)c[1])) {
              c++;
              cval = (cval << 4) | (*c <= '9' ? *c - '0' : _totupper(*c) - 'A' + 10);
            }
          }
          *d++ = cval;
        }
        break;
      default:
        if ('0' <= c[1] && c[1] <= '7') { // octal; no more than three symbols
          c++; int cval = *c - '0';
          if ('0' <= c[1] && c[1] <= '7') c++, cval = (cval << 3) | (*c - '0');
          if ('0' <= c[1] && c[1] <= '7') c++, cval = (cval << 3) | (*c - '0');
          *d++ = cval;
        } else {
          *d++ = *c;
        }
    }
  }
  *d = 0;
  return d - cc;
}

int MakeCEscapes(const TCHAR *c, int l, achar &out) {
  if (!c) { out.l = 0; return 0; }
  int sz = l / sizeof(TCHAR);
  const TCHAR *cc = c, *e = c + l / sizeof(TCHAR);
  for (; cc < e; cc++) if (*cc < ' ') sz += 5; else if (*cc == '\\') sz++;
  out.resize(sz * sizeof(TCHAR)); out.l = sz * sizeof(TCHAR);
  TCHAR *o = out.c;
#ifdef _UNICODE
  static const TCHAR escape[] = _T("\\x%04x");
#else
  static const TCHAR escape[] = _T("\\x%02x");
#endif
  for (cc = c; cc < e; cc++)
    if (*cc < ' ') o += _stprintf(o, escape, (TBYTE)*cc);
    else if (*cc == '\\') *o++ = '\\', *o++ = '\\';
    else *o++ = *cc;
  *o = 0;
  return sz;
}

// return true if processing occured
bool MakeCEscapes(achar &r) {
  if (!r.size()) return false;
  const TCHAR *cc = r.c, *e = r.c + r.l;
  for (; cc < e; cc++) if (*cc < ' ' || *cc == '\\') break;
  if (cc == e) return false;
  TCHAR *c = r.c; r.c = 0;
  MakeCEscapes(c, r.l, r);
  free(c);
  return true;
}

UINT achar::GetDlgItemTextUnCEsc(HWND hwnd, int nIDDlgItem) {
  GetDlgItemText(hwnd, nIDDlgItem);
  return l = ProcessCEscapes((TCHAR *)c, l / sizeof(TCHAR)) * sizeof(TCHAR);
}

int val_ed_dialog_data::EditValue(HWND hwnd) {
  hk=GetKeyByName(keyname,KEY_QUERY_VALUE | KEY_SET_VALUE);
  if ((readonly = (HANDLE)hk==INVALID_HANDLE_VALUE) != 0) {
    hk=GetKeyByName(keyname,KEY_QUERY_VALUE);
  }
  if ((HANDLE)hk==INVALID_HANDLE_VALUE) return 1;
  auto_close_hkey aclhk(hk);
  if (data.QueryValue(hk, name.c, type)) {
    return 1;
  }
  bool ok = false;
  //is_changed = false;
  switch(type) {
  case REG_SZ: case REG_EXPAND_SZ:
    ok = DialogBoxParam(hInst,_T("EDSTRING"),hwnd,EditString,(LPARAM)this) >=0 && newdata.c;
    break;
  case REG_DWORD_BIG_ENDIAN:
    be_le_swap(data.b);
  case REG_DWORD:
    if (data.l < 4) data.resize(4);
    newdata.l = (data.l < 4)? 4 : data.l;
    newdata.resize();
    ok = DialogBoxParam(hInst,_T("EDDWORD"),hwnd,EditDWORD,(LPARAM)this) > 0;
    if (type==REG_DWORD_BIG_ENDIAN) be_le_swap(newdata.b);
    break;
  case REG_MULTI_SZ:
    ok = DialogBoxParam(hInst,_T("EDMSTRING"),hwnd,EditMString,(LPARAM)this) >=0 && newdata.c;
    break;
  default:
    flag1 = type != REG_BINARY;
    ok = DialogBoxParam(hInst,_T("EDBINARY"),hwnd,EditBinary,(LPARAM)this) >=0 && newdata.c;
    break;
  }
  if (!ok) return 1;
  RegSetValueEx(hk, name.c, NULL, type, newdata.b, newdata.l + (type == REG_SZ || type == REG_EXPAND_SZ ? sizeof(TCHAR) : 0));
  return 0;
}

#ifndef _WIN32_WCE
INT_PTR CALLBACK DialogCR(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    connect_remote_dialog_data *cr;
    switch (msg) {
	case WM_INITDIALOG: 
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      SendDlgItemMessage(hwnd, IDC_C_HKLM, BM_SETCHECK, 1, 0);
      SendDlgItemMessage(hwnd, IDC_C_HKCR, BM_SETCHECK, 0, 0);
      SendDlgItemMessage(hwnd, IDC_C_HKUS, BM_SETCHECK, 1, 0);
      SendDlgItemMessage(hwnd, IDC_C_HKPD, BM_SETCHECK, 0, 0);
      SendDlgItemMessage(hwnd, IDC_C_HKDD, BM_SETCHECK, 0, 0);
      EnableWindow(GetDlgItem(hwnd, IDC_C_HKCR), 0);
      EnableWindow(GetDlgItem(hwnd, IDOK), 0);
      SetDlgEditCtrlHist(hwnd, IDC_E_COMP);
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
      cr = (connect_remote_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
      BROWSEINFO bi; fchar namebuf;  IMalloc *shmalloc; LPITEMIDLIST iil;
	  switch (LOWORD (wParam)) {
	  case IDCANCEL: EndDialog(hwnd,0); break; 
	  case IDOK: 
        cr->LM = SendDlgItemMessage(hwnd, IDC_C_HKLM, BM_GETCHECK, 0,0) != 0;
        cr->CR = SendDlgItemMessage(hwnd, IDC_C_HKCR, BM_GETCHECK, 0,0) != 0;
        cr->US = SendDlgItemMessage(hwnd, IDC_C_HKUS, BM_GETCHECK, 0,0) != 0;
        cr->PD = SendDlgItemMessage(hwnd, IDC_C_HKPD, BM_GETCHECK, 0,0) != 0;
        cr->DD = SendDlgItemMessage(hwnd, IDC_C_HKDD, BM_GETCHECK, 0,0) != 0;
        cr->comp.GetDlgItemText(hwnd, IDC_E_COMP);
        EndDialog(hwnd,1); break;
      case IDC_E_COMP:
        if (HIWORD(wParam) == EN_CHANGE) {
          EnableWindow(GetDlgItem(hwnd, IDOK), SendDlgItemMessage(hwnd, IDC_E_COMP, WM_GETTEXTLENGTH,0,0) != 0);
        }
        break;
      case IDC_BROWSECOMP:
        if (!co_initialized) CoInitialize(0), co_initialized = true;
        bi.hwndOwner = hwnd;
        bi.pidlRoot = 0; bi.pszDisplayName = namebuf.c = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
        bi.lpszTitle = _T("Select computer");
#ifndef BIF_NEWDIALOGSTYLE
#define BIF_NEWDIALOGSTYLE     0x0040
#endif
        bi.ulFlags  = BIF_BROWSEFORCOMPUTER /*|BIF_NEWDIALOGSTYLE*/;
        bi.lpfn = 0, bi.lParam = 0;
        SHGetSpecialFolderLocation(hwnd, CSIDL_NETWORK, (ITEMIDLIST **)&bi.pidlRoot);
        iil = SHBrowseForFolder(&bi);
        if (iil) {
          SetDlgItemText(hwnd, IDC_E_COMP, namebuf.c);
        }
        if (bi.pidlRoot || iil) {
          SHGetMalloc(&(shmalloc = 0));
          if (shmalloc) {
            if (bi.pidlRoot) shmalloc->Free((void*)bi.pidlRoot);
            if (iil) shmalloc->Free((void*)iil);
            shmalloc->Release();
          }
        }
        break;
      }
	  return 1;
	}
	return 0;
}

INT_PTR CALLBACK DialogDcR(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
    disconnect_remote_dialog_data *cr;
    int n;
    switch (msg) {
	case WM_INITDIALOG: 
      SetWindowLongPtr(hwnd, DWLP_USER, lParam);
      cr = (disconnect_remote_dialog_data*)lParam;
      for(n = 0; n < cr->numsel; n++) {
        SendDlgItemMessage(hwnd, IDC_L_NAMES, LB_ADDSTRING, 0, (LPARAM)cr->keys[n]);
      }
      return 1;
    case WM_CLOSE: EndDialog(hwnd,0); return 1;
    case WM_COMMAND:
      cr = (disconnect_remote_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
	  switch (LOWORD (wParam)) {
	  case IDCANCEL: EndDialog(hwnd,0); break; 
	  case IDOK: 
        for(n = 0; n < cr->numsel; n++) {
          if (!SendDlgItemMessage(hwnd, IDC_L_NAMES, LB_GETSEL, n, 0)) cr->keys[n] = 0;
        }
        EndDialog(hwnd,1); break;
      case IDC_B_ALL:
        SendDlgItemMessage(hwnd, IDC_L_NAMES, LB_SELITEMRANGEEX, 0, cr->numsel - 1);
        break;
      }
	  return 1;
	}
	return 0;
}

void CheckDisconnRemoteMenuState() {
  bool en = false;
  for(size_t n = 0; n < rkeys.v.size(); n++) if (rkeys.v[n].ki.flags & IS_REMOTE_KEY) en = true;
  EnableMenuItem(theFileMenu, IDM_REGISTRY_DISCONNECT, en? MF_ENABLED : MF_GRAYED);
}

struct regasyncconn {
  const TCHAR *comp;
  HKEY node, *out;
  int retcode;
};
DWORD WINAPI regconnectthread(LPVOID p) {
  regasyncconn *s = (regasyncconn*)p;
  s->retcode = RegConnectRegistry(s->comp, s->node, s->out);
  return s->retcode;
}

int ConnectRegistry(achar &comp, HKEY node, const TCHAR *node_name5, TVINSERTSTRUCT &tvins) {
  if (rr_connecting) return 1;
  HKEY hk;
  TCHAR *kn = (TCHAR*)malloc((comp.l + 6) * sizeof(TCHAR));
  _tcscpy(kn, comp.c); _tcscpy(kn + comp.l, node_name5);
  regasyncconn rasc = { comp.c, node, &hk, -1 };
  {
    fchar s(malloc((comp.l + 40) * sizeof(TCHAR)));
    _stprintf(s.c, _T("Connecting %s ..."), kn);
    SendMessage(SbarW, SB_SETTEXT, 0, (LPARAM)s.c);
  }
  DWORD tid;
  HANDLE h = CreateThread(0, 32768, regconnectthread, &rasc, 0, &tid);
  Sleep(50);
  bool hasquit = false;
  WPARAM quitParam = 0;
  if (!h) {
    rasc.retcode = RegConnectRegistry(comp.c, node, &hk);
  } else {
    rr_connecting = true;
	while (1) {
      DWORD m = MsgWaitForMultipleObjects(1, &h, false, 500, QS_ALLEVENTS);
      MSG msg;
      if (m == WAIT_OBJECT_0) break;
      while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { 
        if (msg.message == WM_QUIT) {
          hasquit = true;
          quitParam = msg.wParam;
          MainWindow = 0;
          continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      } 
	}
    CloseHandle(h);
    rr_connecting = false;
  }
  if (!rasc.retcode) {
    tvins.item.pszText = kn;
    HTREEITEM item = TreeView_InsertItem(TreeW, &tvins);
    if (rkeys.add(kn, hk, CAN_LOAD_SUBKEY | IS_REMOTE_KEY, item)) {
      TreeView_DeleteItem(TreeW, item);
      free(kn);
    }
    SendMessage(SbarW, SB_SETTEXT, 0, (LPARAM)_T("ok"));
  } else {
    ErrMsgDlgBox(kn, rasc.retcode);
    free(kn);
    SendMessage(SbarW, SB_SETTEXT, 0, (LPARAM)_T("failed"));
  }
  if (hasquit) PostQuitMessage(quitParam);
  return rasc.retcode;
}
#endif

void ErrMsgDlgBox(LPCTSTR sss, DWORD le) {
  TCHAR* lpMsgBuf;
  DWORD n=500;
  DWORD LE=le? le : GetLastError();

  FormatMessage( 
	FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	LE,
	0 /*MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)*/, // Default language
	(LPTSTR)&lpMsgBuf,
	0,
	NULL 
	);
  MessageBox( NULL, lpMsgBuf, sss, MB_OK|MB_ICONINFORMATION );
  LocalFree( lpMsgBuf );
}
void ErrMsgDlgBox2(LPCTSTR sss, LPCTSTR sss1, DWORD le) {
  TCHAR* lpMsgBuf;
  DWORD n=500;
  DWORD LE=le? le : GetLastError();

  FormatMessage( 
	FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	LE,
	0 /*MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)*/, // Default language
	(LPTSTR)&lpMsgBuf,
	0,
	NULL 
	);
  fchar m1(malloc((_tcslen(lpMsgBuf) + _tcslen(sss1) + 10) * sizeof(TCHAR)));
  _stprintf(m1.c, _T("%s\nfailed:\n%s"), sss1, lpMsgBuf);
  MessageBox( NULL, m1.c, sss, MB_OK|MB_ICONINFORMATION );
  LocalFree( lpMsgBuf );
}

#ifndef _WIN32_WCE
void CommDlgErrMsgDlgBox(LPCTSTR sss, DWORD le) {
  const TCHAR *c;
  switch(le) {
  case CDERR_DIALOGFAILURE: c = _T("CDERR_DIALOGFAILURE"); break;
  case CDERR_GENERALCODES: c = _T("CDERR_GENERALCODES"); break;
  case CDERR_STRUCTSIZE: c = _T("CDERR_STRUCTSIZE"); break;
  case CDERR_INITIALIZATION: c = _T("CDERR_INITIALIZATION"); break;
  case CDERR_NOTEMPLATE: c = _T("CDERR_NOTEMPLATE"); break;
  case CDERR_NOHINSTANCE: c = _T("CDERR_NOHINSTANCE"); break;
  case CDERR_LOADSTRFAILURE: c = _T("CDERR_LOADSTRFAILURE"); break;
  case CDERR_FINDRESFAILURE: c = _T("CDERR_FINDRESFAILURE"); break;
  case CDERR_LOADRESFAILURE: c = _T("CDERR_LOADRESFAILURE"); break;
  case CDERR_LOCKRESFAILURE: c = _T("CDERR_LOCKRESFAILURE"); break;
  case CDERR_MEMALLOCFAILURE: c = _T("CDERR_MEMALLOCFAILURE"); break;
  case CDERR_MEMLOCKFAILURE: c = _T("CDERR_MEMLOCKFAILURE"); break;
  case CDERR_NOHOOK: c = _T("CDERR_NOHOOK"); break;
  case CDERR_REGISTERMSGFAIL: c = _T("CDERR_REGISTERMSGFAIL"); break;
  case PDERR_PRINTERCODES: c = _T("PDERR_PRINTERCODES"); break;
  case PDERR_SETUPFAILURE: c = _T("PDERR_SETUPFAILURE"); break;
  case PDERR_PARSEFAILURE: c = _T("PDERR_PARSEFAILURE"); break;
  case PDERR_RETDEFFAILURE: c = _T("PDERR_RETDEFFAILURE"); break;
  case PDERR_LOADDRVFAILURE: c = _T("PDERR_LOADDRVFAILURE"); break;
  case PDERR_GETDEVMODEFAIL: c = _T("PDERR_GETDEVMODEFAIL"); break;
  case PDERR_INITFAILURE: c = _T("PDERR_INITFAILURE"); break;
  case PDERR_NODEVICES: c = _T("PDERR_NODEVICES"); break;
  case PDERR_NODEFAULTPRN: c = _T("PDERR_NODEFAULTPRN"); break;
  case PDERR_DNDMMISMATCH: c = _T("PDERR_DNDMMISMATCH"); break;
  case PDERR_CREATEICFAILURE: c = _T("PDERR_CREATEICFAILURE"); break;
  case PDERR_PRINTERNOTFOUND: c = _T("PDERR_PRINTERNOTFOUND"); break;
  case PDERR_DEFAULTDIFFERENT: c = _T("PDERR_DEFAULTDIFFERENT"); break;
  case CFERR_CHOOSEFONTCODES: c = _T("CFERR_CHOOSEFONTCODES"); break;
  case CFERR_NOFONTS: c = _T("CFERR_NOFONTS"); break;
  case CFERR_MAXLESSTHANMIN: c = _T("CFERR_MAXLESSTHANMIN"); break;
  case FNERR_FILENAMECODES: c = _T("FNERR_FILENAMECODES"); break;
  case FNERR_SUBCLASSFAILURE: c = _T("FNERR_SUBCLASSFAILURE"); break;
  case FNERR_INVALIDFILENAME: c = _T("FNERR_INVALIDFILENAME"); break;
  case FNERR_BUFFERTOOSMALL: c = _T("FNERR_BUFFERTOOSMALL"); break;
  case FRERR_FINDREPLACECODES: c = _T("FRERR_FINDREPLACECODES"); break;
  case FRERR_BUFFERLENGTHZERO: c = _T("FRERR_BUFFERLENGTHZERO"); break;
  case CCERR_CHOOSECOLORCODES: c = _T("CCERR_CHOOSECOLORCODES"); break;
  default: c = _T("Unknown common dialogs error");
  }
  MessageBox( NULL, c, sss, MB_OK|MB_ICONINFORMATION );
}

int EnablePrivilege_NT(LPCTSTR where, LPCTSTR name) {
	LUID luidSD;
	TOKEN_PRIVILEGES tp;
    HANDLE hCurProc, CurProcToken;
	hCurProc = GetCurrentProcess();
	if (!OpenProcessToken(hCurProc, TOKEN_ADJUST_PRIVILEGES, &CurProcToken)) return 1;
    auto_close_handle A(CurProcToken);
    if (!LookupPrivilegeValue(where, name, &luidSD)) return 2;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luidSD;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(CurProcToken, false, &tp, 0, NULL, NULL)) return 3;
    return 0;
}
#endif

#if 0
class MySecur : public ISecurityInformation {
public:
  HRESULT GetObjectInformation(PSI_OBJECT_INFO pObjectInfo);
  HRESULT GetSecurity(SECURITY_INFORMATION RequestedInformation, PSECURITY_DESCRIPTOR* ppSecurityDescriptor, BOOL fDefault);
  HRESULT SetSecurity(SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor);
  HRESULT GetAccessRights(const GUID* pguidObjectType, DWORD dwFlags, PSI_ACCESS* ppAccess, ULONG* pcAccesses, ULONG* piDefaultAccess);
  HRESULT MapGeneric(const GUID* pguidObjectType, UCHAR* pAceFlags, ACCESS_MASK* pMask);
  HRESULT GetInheritTypes(PSI_INHERIT_TYPE* ppInheritTypes, ULONG* pcInheritTypes);
  HRESULT PropertySheetPageCallback(HWND hwnd, UINT uMsg, SI_PAGE_TYPE uPage);
};
int EditKeySecurity(const char *name) {
  //EnablePrivilege_NT(0/*comp name!*/, SE_SECURITY_NAME);
  HKEY hk = GetKeyByName(name, KEY_EXECUTE | STANDARD_RIGHTS_READ /*| ACCESS_SYSTEM_SECURITY*/ /*| WRITE_DAC | WRITE_OWNER*/);
  //DisablePrivilege_NT(0/*comp name!*/, SE_SECURITY_NAME);
  if (hk == INVALID_HANDLE_VALUE) return 1;
  // To read the SACL from the security descriptor, the calling process must have been granted ACCESS_SYSTEM_SECURITY access when the key was opened. The proper way to get this access is to enable the SE_SECURITY_NAME privilege in the caller's current token, open the handle for ACCESS_SYSTEM_SECURITY access, and then disable the privilege.
  //RegGetKeySecurity(hk,,, &sz);
  //RegSetKeySecurity(hk,,);
  //MySecur mysecur;
  //EditSecurity(MainWindow, &mysecur);
  return 0;
}
#endif

void AddKeyToFavorites(fchar &title, fchar &key, fchar &value, fchar &comment) {
  HKEY hk;
  DWORD LE = RegCreateKeyEx(HKEY_CURRENT_USER, REFAVPATH, 0, 0, 0, KEY_SET_VALUE, 0, &hk, 0);
  if (LE) {
    ErrMsgDlgBox2(_T("Could not save favorite!"), _T("RegOpenKeyEx for key\n") REFAVPATH _T(""), LE);
    return;
  }
  const TCHAR *d, *s = rkeys.ShortName2LongName(key.c, &d);
  fchar t;
  int vaddl = (value.c? _tcslen(value.c) + 3 : 0) + (comment.c? _tcslen(comment.c) + 4 : 0);
  if (s && d - key.c == 5) { // i.e. local
    t.c = (TCHAR*)malloc((MYCOMPLEN + _tcslen(s) + _tcslen(d) + 2 + vaddl) * sizeof(TCHAR));
    _stprintf(t.c, MYCOMP _T("%s\\%s"), s, d);
  } else { // remote?
    if (vaddl) t.c = (TCHAR*)malloc((_tcslen(key.c) + vaddl) * sizeof(TCHAR));
  }
  if (vaddl) {
    TCHAR *e = t.c + _tcslen(t.c) + 1;
    if (value.c) { _tcscpy(e, value.c); e += _tcslen(value.c) + 1; }
    else {
      if (comment.c) *e++ = 1;
      *e++ = 0;
    }
    if (comment.c) { _tcscpy(e, comment.c); e += _tcslen(comment.c) + 1; }
    else *e++ = 0;
    *e++ = 0; //2nd sero
    vaddl = e - t.c;
  }
  
  DWORD type = vaddl? REG_MULTI_SZ : REG_SZ;
  TCHAR *key2 = t.c? t.c : key.c;
  LE = RegSetValueEx(hk, title.c, NULL, type, (BYTE*)key2, (vaddl? vaddl : _tcslen(key2) + 1) * sizeof(TCHAR));
  if (LE) {
    ErrMsgDlgBox2(_T("Could not save favorite!"), _T("RegSetValueEx for key\n") REFAVPATH _T(""), LE);
    RegCloseKey(hk);
    return;
  }
  RegCloseKey(hk);
}

void FavKeyName2ShortName(const TCHAR *k0, fchar &key) {
  if (!_tcsnicmp(k0, MYCOMP, MYCOMPLEN)) {
    FixKeyName(k0 + MYCOMPLEN, key);
    return;
  }
  // ????
  TCHAR *t = _tcsdup(k0);
  free(key.c);
  key.c = t;
}

void AddFavoritesToMenu(HMENU menu) {
  HKEY hk;
  DWORD LE = RegOpenKeyEx(HKEY_CURRENT_USER, REFAVPATH, 0, KEY_EXECUTE | KEY_QUERY_VALUE, &hk);
  if (LE) return;
  value_iterator i(hk);
  if (i.err()) {
    RegCloseKey(hk);
    return;
  }
  while(RemoveMenu(menu, 4, MF_BYPOSITION)); //clear the menu
  DWORD p = 41000; // menu item id's
  for(vector<favitem_t>::iterator j = favItems.begin(); j != favItems.end(); j++) {
    free(j->name);  free(j->key);
    free(j->value); free(j->comment);
  }
  favItems.clear();
  for (; !i.end(); i++) {
	if (i.is_ok && (i.type == REG_SZ || i.type == REG_MULTI_SZ)) {
      //(i.data, i.data.l);
      AppendMenu(menu, MF_STRING, p++, i.name.c);
      favitem_t fit = { _tcsdup(i.name.c), _tcsdup(i.data.c), 0, 0 };
      if (i.type == REG_MULTI_SZ) {
        if (*(fit.value = _tcschr(i.data.c, 0) + 1) && fit.value < i.data.c + i.data.l) {
          fit.comment = _tcschr(fit.value, 0) + 1;
          if (*fit.value == 1) fit.value = 0;
          if (fit.comment >= i.data.c + i.data.l || !*fit.comment) fit.comment = 0;
        } else {
          fit.value = 0;
        }
      }
      if (fit.value) fit.value = _tcsdup(fit.value);
      if (fit.comment) fit.comment = _tcsdup(fit.comment);
      favItems.push_back(fit);
      if (p >= 41100) break; //only 100 items allowed
    }
  }
  RegCloseKey(hk);
}

void AddSelCommandsToMenu(HWND hwnd, HMENU pum) {
  while (RemoveMenu(pum, 0, MF_BYPOSITION)); //clear the menu
  DWORD cnc = MF_ENABLED;
  HKEY hk = GetKeyByName(currentitem, KEY_QUERY_VALUE | KEY_SET_VALUE);
  if ((HANDLE)hk == INVALID_HANDLE_VALUE) {
    hk = GetKeyByName(currentitem, KEY_QUERY_VALUE);
    if ((HANDLE)hk == INVALID_HANDLE_VALUE) return; //Crash
    cnc = MF_GRAYED;
  }
  if (hwnd == TreeW) {
    TVITEM item;
    item.hItem = TreeView_GetSelection(TreeW);
    item.mask = TVIF_CHILDREN | TVIF_STATE;
    item.stateMask = TVIS_EXPANDED | TVIS_EXPANDEDONCE;
    TreeView_GetItem(TreeW, &item);
    AppendMenu(pum, item.cChildren ? MF_ENABLED : MF_GRAYED,
        330 - ((item.state & TVIS_EXPANDED) !=0 ),
        (item.state & TVIS_EXPANDED) ? _T("Collapse") : _T("Expand"));
    HMENU mn1 = TypeMenu(300, 0, cnc);
    AppendMenu(pum, MF_POPUP, (UINT_PTR)mn1, _T("&New"));
    AppendMenu(pum, MF_STRING | cnc, 331, _T("&Find..."));
    AppendMenu(pum, MF_SEPARATOR, -1, _T(""));
    AppendMenu(pum, MF_STRING | cnc, 332, _T("&Delete"));
    AppendMenu(pum, MF_STRING | cnc, 333, _T("&Rename"));
    AppendMenu(pum, MF_STRING | cnc, 334, _T("&Move to..."));
    AppendMenu(pum, MF_STRING, 335, _T("Cop&y to..."));
    AppendMenu(pum, MF_SEPARATOR, -1, _T(""));
    AppendMenu(pum, MF_STRING, 336, _T("&Copy key name (full)"));
    AppendMenu(pum, MF_STRING, 337, _T("Copy key name (&short)"));
#ifndef _WIN32_WCE
    SetMenuDefaultItem(pum, 0, MF_BYPOSITION);
#endif
  } else if (hwnd == ListW) {
    DWORD type, ns;
    if (RegQueryInfoKey(hk,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&ns,NULL,NULL,NULL)!= ERROR_SUCCESS) {
      RegCloseKey(hk);
      return;
    }
    if (ListView_GetSelectedCount(ListW)) {
      TCHAR *name = (TCHAR*)malloc((ns += 30) * sizeof(TCHAR));
      *name = 0;
      int i = ListView_GetNextItem(ListW, -1, LVNI_FOCUSED);
      ListView_GetItemText(ListW, i, 0, name, ns);
      RegQueryValueEx(hk, name, NULL, &type, NULL, NULL);
      free(name);
      int type_n = GetTypeMnuNo(type);
      HMENU mn2 = TypeMenu(350, type_n, cnc);
      AppendMenu(pum, MF_STRING, 320, _T("&Modify"));
      AppendMenu(pum, MF_POPUP, (UINT_PTR)mn2, _T("&Change Type"));
      AppendMenu(pum, MF_SEPARATOR, -1, _T(""));
      AppendMenu(pum, MF_STRING | cnc, 321, _T("&Delete"));
      AppendMenu(pum, MF_STRING | cnc, 322, _T("&Rename"));
      AppendMenu(pum, MF_STRING | cnc, 323, _T("Move &to..."));
      AppendMenu(pum, MF_STRING, 324, _T("Cop&y to..."));
      AppendMenu(pum, MF_SEPARATOR, -1, _T(""));
#ifndef _WIN32_WCE
      SetMenuDefaultItem(pum, 0, MF_BYPOSITION);
#endif
    }
    HMENU mn1 = TypeMenu(300, 0, cnc);
    AppendMenu(pum, MF_POPUP, (UINT_PTR)mn1, _T("&New"));
  }
  CloseKey_NHC(hk);
}

const int MAX_KEY_TITLE_LEN = 24;
void SuggestTitleForKey(const TCHAR *key, fchar &title) {
  int l = _tcslen(key);
  if (l <= MAX_KEY_TITLE_LEN) {
    title.c = _tcsdup(key);
    return;
  }
  title.c = (TCHAR*)malloc((MAX_KEY_TITLE_LEN + 1) * sizeof(TCHAR));
  memcpy(title.c, key, 4 * sizeof(TCHAR));
  memcpy(title.c + 4, _T("..."), 3 * sizeof(TCHAR));
  _tcscpy(title.c + 7, key + (l - MAX_KEY_TITLE_LEN + 7));
}
