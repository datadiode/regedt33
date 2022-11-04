#ifndef _REGEDIT33_H_
#define _REGEDIT33_H_
#pragma once

#include <windows.h>
#include "wince.h"
#include <winnt.h> // TBYTE
#include <tchar.h>
#include <ctype.h>
#include <commctrl.h>

#include <algorithm>
#include <unordered_set>
#include <unordered_map>

using namespace std;

#include "automgm.h"

/// Iterator for values of a registry key
struct value_iterator {
  bool is_end, is_err, is_ok;
  DWORD num_val, cur_val;
  achar name, data;
  DWORD type;
  HKEY hk;
  value_iterator(HKEY hk_) : is_end(1), is_err(0), is_ok(0), num_val(0), cur_val(0), hk(hk_) {
    DWORD l_name, l_data;
    if ((HANDLE)hk == INVALID_HANDLE_VALUE
      || RegQueryInfoKey(hk,0,0,0,0,0,0,&num_val,&l_name,&l_data,NULL,NULL)) {
      is_err = true;
      return;
    }
    name.resize(l_name * sizeof(TCHAR)); 
    data.resize(l_data); //??
    if (num_val > 0) get(), is_end = false;
  }
  void get() {
    name.l = name.s, data.l = data.s;
    is_ok = !RegEnumValue(hk, cur_val, name.c, &name.l, NULL,&type, data.b, &data.l);
    name.l *= sizeof(TCHAR);
  }
  void operator ++(int) {
    if (++cur_val < num_val) get();
    else is_end = true;
  }
  bool end() { return is_end; }
  bool err() { return is_err; }
private:
  void operator =(const value_iterator&);
};


int DisplayOFNdlg(achar &name, const TCHAR *title, const TCHAR *filter, bool no_RO = true, bool for_save = false);
int SetDlgEditCtrlHist(HWND hwnd, int item);
int SetEditCtrlHistAndText(HWND hwnd, int item, const TCHAR *c);
void LoadSettings();
void ErrMsgDlgBox(LPCTSTR, DWORD le = 0);
const TCHAR* TypeCName(DWORD type);
#ifndef _WIN32_WCE
void CommDlgErrMsgDlgBox(LPCTSTR, DWORD);
int EnablePrivilege_NT(LPCTSTR where, LPCTSTR name);
extern bool has_rest_priv, has_back_priv;
#endif
void getDlgItemText(TCHAR *&var, HWND hwnd, int ctrl);
void he_DefaultWindowSize(int &x, int &y);

extern HINSTANCE hInst;
extern HWND MainWindow, SbarW, hwndToolTip;

//----- keytools.cpp -----
  /// Caller must free handle: ///
HKEY GetKeyByName(const TCHAR*,REGSAM);
  /// Caller must free handle: ///
HKEY CreateKeyByName(const TCHAR *c,const TCHAR *cls,REGSAM samDesired);
BOOL CloseKey_NHC(HKEY);

extern LONG lastRegErr;
  /// Caller must free memory: ///
TCHAR *GetKeyNameByItem(HWND,HTREEITEM);
  /// Caller must free handle: ///
HKEY GetKeyByItem(HWND,HTREEITEM,REGSAM);
BOOL CanKeyBeRenamed(HWND,HTREEITEM);
HTREEITEM ShowItemByKeyName(HWND tv, const TCHAR *key);
int SelectItemByValueName(HWND lv, const TCHAR *name);

int RenameKeyValue(HKEY hk, HKEY hk2, const TCHAR *newname, const TCHAR *oldname, bool copy = false);
BOOL IsSubKey(const TCHAR*,const TCHAR*);
int CopyKey(const TCHAR*,const TCHAR*,const TCHAR*);
int MoveKey(const TCHAR*,const TCHAR*);
int DeleteAllSubkeys(HKEY);
int DeleteKeyByName(TCHAR*);
bool CanDeleteThisKey(const TCHAR *name, bool qConfig);

void GetValueDataString(char*, TCHAR*,int,DWORD);

struct str_equal_to { bool operator()(const TCHAR *x, const TCHAR *y) const { return !_tcscmp(x,y); } };
struct str_iequal_to { bool operator()(const TCHAR *x, const TCHAR *y) const { return !_tcsicmp(x,y); } };
struct hash_str { size_t operator()(const TCHAR* s) const {
  unsigned long h = 0;
  for ( ; *s; ++s) h = 5 * h + (TBYTE)*s;
  return size_t(h);
}};
struct hash_stri { size_t operator()(const TCHAR* s) const {
  unsigned long h = 0;
  for ( ; *s; ++s) h = 5 * h + toupper((TBYTE)*s);
  return size_t(h);
}};

template<>
struct hash<HKEY> { size_t operator()(HKEY h) const { return (size_t)h; } }; //8-)

struct confirm_replace_dialog_data {
    const TCHAR *keyname, *oldvalue, *newvalue, *olddata;
    int olddatalen;
    const TCHAR *newdata;
    int newdatalen;
    bool def_next;
};

struct val_ed_dialog_data {
  const TCHAR *keyname;
  fchar name;
  achar data, newdata;
  DWORD type;
  HKEY hk;
  bool readonly/*, is_changed*/, flag1;
  int applyEn;
  int EditValue(HWND hwnd);
};
inline void be_le_swap(unsigned char *c) {
  unsigned char tmp; 
  tmp=c[0], c[0]=c[3]; c[3]=tmp;
  tmp=c[1], c[1]=c[2]; c[2]=tmp;
}

enum RKI_FLAGS {
  CAN_LOAD_SUBKEY = 1,
  IS_REMOTE_KEY   = 2,
  CANT_REMOVE_KEY = 4
};
struct RootHandleInfo_type {
  HKEY hkey;
  const TCHAR *full_name;
  int flags;
  HTREEITEM item;
};
struct HKRootName2Handle_item_type {
  const TCHAR *name;
  RootHandleInfo_type ki;
};
typedef unordered_map<const TCHAR*, RootHandleInfo_type, hash_stri, str_iequal_to> n2kmap;
typedef unordered_map<HKEY, const TCHAR*> k2nmap;
typedef unordered_map<const TCHAR*, const TCHAR *, hash_stri, str_iequal_to> namap;
extern struct rk_init_s {
  n2kmap n2k;
  k2nmap k2n;
  namap  lk2sk;
  vector<HKRootName2Handle_item_type> v;
  rk_init_s();
  size_t size() const { return v.size(); }
  HKEY KeyByName(const TCHAR *name, const TCHAR **out = 0);
  const TCHAR *ShortName2LongName(const TCHAR *name, const TCHAR **out);
  const TCHAR *LongName2ShortName(const TCHAR *name, const TCHAR **out);
  int add(const TCHAR *name, HKEY hkey, int flags, HTREEITEM item);
  int remove(const TCHAR *name, HKRootName2Handle_item_type &out);
} rkeys;

//--end keytools.cpp -----

#endif //_REGEDIT33_H_
