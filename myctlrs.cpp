#include <windows.h>
#include "wince.h"
#include <tchar.h>
#include <assert.h>

#include <algorithm>
#include <map>
#include <vector>

using namespace std;

#include "automgm.h"

/* This file contains:
 * 1) Hexadecimal Binary Editor
 * 2) History in edit controls
 * 3) Some unused(!) draft code of an unfinished 
 *    attempt to make my own TreeView control
 */

extern HINSTANCE hInst;
extern HFONT Cour12;


//++++ Hexadecimal Binary Editor +++++++++++++++++++++++++++++++++++++++++++++
UINT hcf=RegisterClipboardFormat(_T("RegEdit_HexData"));

void hexbyte(BYTE b,TCHAR *dst) {
  static const TCHAR tt[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
  dst[0]=tt[b>>4];
  dst[1]=tt[b&15];
}
BYTE HexDigVal(TCHAR c) {
  if (c>='0' && c<='9') return c-'0';
  if (c>='A' && c<='F') return c-'A'+10;
  if (c>='a' && c<='f') return c-'a'+10;
  return 0;
}
const BYTE invchrep='.';
const BYTE cursortype=0; //vert

const int he_charheight = 14, he_charwidth = 8;
const int he_defnumlines = 8, he_defnumcolumns = 8;
void he_DefaultWindowSize(int &x, int &y) {
  x = he_charwidth * (he_defnumcolumns * 4 + 7);
  y = he_charheight * he_defnumlines + 5;
}

struct undo_struct { DWORD action, start, len, offs; };

struct HexEditorState {
  HWND hwnd; // No need to pass hwnd as function params
  HWND scrollwnd;
  DWORD cursorat;
  bool cursattext;
  BYTE cursorinpos;
  bool decimalflag;
  BYTE decimalbyte;
  DWORD fvline; //first visible line;
  DWORD numlines; //number of lines;
  DWORD numcolumns;
  DWORD undopos;
  vector<char> undobuf;
  vector<undo_struct> undoact;
  DWORD selectionstart;
  DWORD selectionend;
  bool readonly, undo_runs, undo_disabled;
  bool isblockselected, specialcursorpos;
  int currscrollrange, currscrollpos;

  char *dataptr; //Pointer to data
  DWORD datalen; //Data length

  HexEditorState(HWND hwnd_) {
	memset(this, 0, sizeof(*this));
    hwnd = hwnd_;
	numlines = he_defnumlines; numcolumns = he_defnumcolumns;
	dataptr = 0;
	datalen = 0;
	undo_disabled = true; // has issues, therefore disable
  }
  ~HexEditorState() {
    free(dataptr);
  }
  bool HEXEdit_SetCaretPos(bool is_inv = false, bool no_fw_upd = false);
  DWORD he_MouseAt(short m_x, short m_y, BYTE marksbe);
  void he_InsByte(BYTE b);
  void he_InsBytes(BYTE *b, int num);
  void he_DelByte();
  void he_DelSel();

  enum actions { ADD_BYTES = 1, DELETE_BYTES = 2 };
  void RegAction(DWORD type, DWORD start, const char *data, DWORD len);
  void FixAdd1Action(BYTE data);
  void Undo();
  void Redo();
  void EnableUndo(bool enable);

  void he_onchange(WORD w);
  int setScrollRange();
  int setScrollPos(bool redraw = false);
  int vscroll(WPARAM);
  void Paint();
  void onChar(WPARAM wParam);
  void onKeyDown(WPARAM wParam);
  void onKeyUp(WPARAM wParam);
  void EditMenu(LPARAM lParam);
  void CutOrCopy(bool cut);
  void Paste();
};

void HexEditorState::RegAction(DWORD type, DWORD start, const char *data, DWORD len) {
  if (undo_runs || undo_disabled) return;
  size_t sz;
  if (undopos > undoact.size()) undopos = undoact.size();
  if (undopos == undoact.size()) sz = undobuf.size();
  else undobuf.resize(sz = undoact[undopos].offs);
  undo_struct us = { type, start, len,  sz };
  undoact.resize(undopos++);
  undoact.push_back(us);
  //save added or deleted bytes
  undobuf.resize(sz + len);
  memcpy(&(undobuf[sz]), data, len);
}
void HexEditorState::FixAdd1Action(BYTE data) {
  if (undo_runs || undo_disabled || !undopos || undopos != undoact.size()) return; //paranoia
  undo_struct &u = undoact[undopos - 1];
  if (u.len != 1) return;
  undobuf[u.offs] = data;
}
void HexEditorState::Undo() {
  if (undo_disabled || !undopos) return;
  undo_runs = true;
  undo_struct &u = undoact[--undopos];
  if (u.action == ADD_BYTES) {
    selectionstart = u.start, selectionend = u.start + u.len, isblockselected = true;
    cursorat = u.start;
    he_DelSel(); // :)
  } else if (u.action == DELETE_BYTES) {
    cursorat = u.start;
    he_InsBytes((BYTE*)(&undobuf.front() + u.offs), u.len);
    cursorat = u.start + u.len;
  }
  undo_runs = false;
  he_onchange(EN_CHANGE);
}
void HexEditorState::Redo() {
  if (undo_disabled || undopos >= undoact.size()) return;
  undo_runs = true;
  undo_struct &u = undoact[undopos++];
  if (u.action == DELETE_BYTES) {
    selectionstart = u.start, selectionend = u.start + u.len, isblockselected = true;
    cursorat = u.start;
    he_DelSel(); // :)
  } else if (u.action == ADD_BYTES) {
    cursorat = u.start;
    he_InsBytes((BYTE*)(&undobuf.front() + u.offs), u.len);
    cursorat = u.start + u.len;
  }
  undo_runs = false;
  he_onchange(EN_CHANGE);
}
void HexEditorState::EnableUndo(bool enable) {
  undo_disabled = !enable;
  if (!enable) {
    undoact.clear();
    undobuf.clear();
    undopos = 0;
  }
}

bool HexEditorState::HEXEdit_SetCaretPos(bool is_inv, bool no_fw_upd) {
  int x,y;
  DWORD curline = cursorat / numcolumns;
  if (specialcursorpos) 
      if (cursorinpos != 2 && cursorat % numcolumns == 0 && curline > 0) curline--;
      else specialcursorpos = false;
  if (!no_fw_upd) {
    if (curline >= fvline + numlines) fvline = curline - numlines + 1;
    else if (curline < fvline) fvline = curline;
    else is_inv = true;
    setScrollPos(true);
    if (!is_inv) InvalidateRect(hwnd,NULL,1);
  }
  
  switch (cursorinpos) {
  case 0: x = specialcursorpos? numcolumns * 3 + 5 : (cursorat % numcolumns) * 3 + 6; break;
  case 1: x = (specialcursorpos? numcolumns : cursorat % numcolumns) + 7 + 3 * numcolumns; break;
  case 2: x = (cursorat % numcolumns) * 3 + 6 + 1; break;
  default: return 0;
  }
  x *= he_charwidth;
  y = (curline - fvline) * he_charheight + 1;
  return SetCaretPos(x,y) != 0;
}
DWORD HexEditorState::he_MouseAt(short m_x, short m_y, BYTE marksbe) {
  int x = m_x / he_charwidth - 6,   f, rip,
      y= (m_y - 1) / he_charheight;
  DWORD p, d = datalen; const int nc3 = numcolumns * 3;
  if (x < nc3 + 1) f = 0, x = x / 3 + (x == nc3 - 1);
  else f = 1, x -= nc3 + 1;
  if (x >= numcolumns) x = numcolumns, specialcursorpos = true;
  x=(x<0)?0:x;
  rip=x+y*numcolumns,p=fvline*numcolumns;
  if (rip<0) p=(p<(DWORD)-rip) ? 0:p+rip;
  else p=(d-p<(DWORD)rip) ? d:p+rip;
  if (p > d) p = d;
  cursorat=p,cursorinpos=f;
  HEXEdit_SetCaretPos();
  if (marksbe==1) selectionstart=p;
  else if (marksbe==2) selectionend=p;
  return p;
}

void HexEditorState::he_InsByte(BYTE b) {
  char *c = dataptr;
  DWORD d = datalen;
  c=(char*)realloc(c,++d);
  if (!c) return; //must not happen!
  RegAction(ADD_BYTES, cursorat, (char*)&b, 1);
  dataptr = c;
  datalen = d;
  for(d--; d > cursorat; d--) c[d] = c[d - 1];
  c[d] = b;
  setScrollRange(); setScrollPos(true);
}
void HexEditorState::he_InsBytes(BYTE *b, int num) {
  if (num<=0) return;
  char *c = dataptr;
  DWORD d = datalen;
  d+=num;
  c=(char*)realloc(c,d);
  if (!c) return;//must not happen!
  RegAction(ADD_BYTES, cursorat, (char*)b, num);
  dataptr = c;
  datalen = d;
  DWORD e=cursorat+num;
  for(d--; d >= e; d--) c[d] = c[d - num];
  memcpy(c+cursorat,b,num);
  setScrollRange(); setScrollPos(true);
}

void HexEditorState::he_DelByte() {
  DWORD n;
  char *c = dataptr;
  DWORD d = datalen;
  if (!c || cursorat>=d || d==0) return;
  RegAction(DELETE_BYTES, cursorat, c + cursorat, 1);
  d--;
  datalen = d;
  for(n=cursorat;n<d;n++) c[n]=c[n+1];
  setScrollRange(); setScrollPos(true);
}
void HexEditorState::he_DelSel() {
  DWORD sels = min(selectionstart, selectionend), sele = max(selectionstart, selectionend);
  if (!isblockselected || sels == sele) return;
  DWORD n, num = sele - sels;
  char *c = dataptr;
  DWORD d = datalen;
  if (!c || sele > d || d == 0) return;
  RegAction(DELETE_BYTES, sels, c + sels, num);
  d -= num;
  datalen = d;
  for(n = sels; n < d; n++) c[n] = c[n + num];
  isblockselected = false;
  if (cursorat >= sele) cursorat -= num;
  else if (cursorat >= sels) cursorat = sels;
  selectionstart = selectionend = cursorat;
  setScrollRange(); setScrollPos(true);
}

void HexEditorState::he_onchange(WORD w) {
  HEXEdit_SetCaretPos(1);
  InvalidateRect(hwnd,NULL,1);
  UpdateWindow(hwnd);
  HWND pw=GetParent(hwnd);
  if (pw) SendMessage(pw,WM_COMMAND,GetWindowLong(hwnd,GWL_ID)|(w<<16),(LPARAM)hwnd);
}

#ifndef WM_MOUSEWHEEL
  #define WM_MOUSEWHEEL                   0x020A
#endif

inline int HexEditorState::setScrollPos(bool redraw) {
  if (fvline == currscrollpos) return 0;
  currscrollpos = fvline;
  //return SendMessage(scrollwnd, SBM_SETPOS, sp, redraw);
  return SetScrollPos(hwnd, SB_VERT, fvline, redraw);
}
int HexEditorState::setScrollRange() {
  int sr = datalen / numcolumns - numlines + 1; if (sr < 0) sr = 0;
  if (sr == currscrollrange) return 0;
  currscrollrange = sr;
  if (!sr && fvline) { fvline = 0; setScrollPos(false); }
  //return SendMessage(scrollwnd, SBM_SETRANGE, 0, sr);
  return SetScrollRange(hwnd, SB_VERT, 0, sr, false);
}
int HexEditorState::vscroll(WPARAM wParam) {
  int fv = fvline;
  int sr = datalen / numcolumns;
  SCROLLINFO si;
  switch(LOWORD(wParam)) {
  case SB_LINEDOWN: fv++; break;
  case SB_LINEUP: fv--; break;
  case SB_PAGEDOWN: fv += numlines; break;
  case SB_PAGEUP: fv -= numlines; break;
  case SB_BOTTOM: fv = sr; break;
  case SB_TOP: fv = 0; break;
  case SB_THUMBTRACK: case SB_THUMBPOSITION:
    si.cbSize = sizeof (si);
    si.fMask  = SIF_ALL;
    GetScrollInfo (hwnd, SB_VERT, &si);
    fv += si.nTrackPos - currscrollpos;
    break;
  }
  fv = fv > (int)(sr - numlines)? sr - numlines + 1 : fv;
  fv = fv < 0? 0 : fv;
  if (fv != fvline) {
    fvline = fv, currscrollpos = fv;
    SetScrollPos(hwnd, SB_VERT, fv, true);
    HEXEdit_SetCaretPos(true, true);
    InvalidateRect(hwnd,NULL,1);
    return 0;
  }
  return 1;
}

void HexEditorState::Paint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  SelectObject(hdc,Cour12);
  //SetTextColor(hdc, RGB(0,0,0));
  if (readonly) SetBkColor(hdc, RGB(192,192,192));
  else SetBkColor(hdc, RGB(255,255,255));
  DWORD sels = min(selectionstart, selectionend), sele = max(selectionstart, selectionend);
  if (dataptr) {
    int n, k;
    TCHAR *pos;
    BYTE *c = (BYTE *)dataptr;
    /*!*/if (numcolumns<2) numcolumns=2;
    DWORD d = datalen, e = d / numcolumns + 1;
    fchar s(malloc((15 + numcolumns * 4) * sizeof(TCHAR)));
    //MoveToEx(hdc,0,0,NULL);
    //LineTo(hdc, numlines*14+20, numlines*14+20);
    for(n = fvline; n < fvline + numlines && n < e; n++) {
      DWORD gp = n * numcolumns, pgp = gp;
      _stprintf(s.c, _T("%04X  "), gp); pos = s.c + 6;
      for(k=0;k<numcolumns && gp<d;k++,gp++) {
        hexbyte(c[gp],pos); pos[2]=' ';pos+=3;
      }
	  TCHAR *sop = pos + 3 * (numcolumns - k) + 1;
	  std::fill(pos, sop, ' ');
	  pos = sop;
      gp=pgp;
      for(k=0;k<numcolumns && gp<d;k++,gp++) {
        *(pos++)=(c[gp]<32)?invchrep:c[gp];
      }
      *pos=0;
      vector<int> const dx(_tcslen(s.c), he_charwidth);
      ExtTextOut(hdc, 1, (n - fvline) * he_charheight, 0, NULL, s.c, dx.size(), &dx.front());
      if (isblockselected) {
        int lss = max(pgp, sels) - pgp, lse = min(gp, sele) - pgp;
        if (lss < lse) {
          RECT rect = { 0, (n - fvline) * he_charheight, 0, (n + 1 - fvline) * he_charheight };
          rect.left = (lss * 3 + 6) * he_charwidth, rect.right = (lse * 3 + 6) * he_charwidth;
          InvertRect(hdc, &rect);
          rect.left = (lss + 7 + 3 * numcolumns) * he_charwidth, rect.right = (lse + 7 + 3 * numcolumns) * he_charwidth;
          InvertRect(hdc, &rect);
        }
      }
    }
    MoveToEx(hdc,he_charwidth*5,0,NULL);
    LineTo(hdc,he_charwidth*5,(n-fvline)*he_charheight);
    MoveToEx(hdc,he_charwidth*(6+numcolumns*3),0,NULL);
    LineTo(hdc,he_charwidth*(6+numcolumns*3),(n-fvline)*he_charheight);
  }
  EndPaint(hwnd,&ps);
}

void HexEditorState::onKeyDown(WPARAM wParam) {
  DWORD d = datalen, e, at = cursorat;
  bool shift_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

  if (!ctrl_down || cursorinpos != 0) {
    decimalflag = false;
  } else if (wParam >= '0' && wParam <= '9' || wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
    if (!decimalflag) {
      decimalflag = true;
      decimalbyte = 0;
    }
    decimalbyte *= 10;
    decimalbyte += static_cast<BYTE>(wParam & 0xF);
  }

  bool need_at = true;
  if ((e=cursorinpos)==2) cursorinpos=0;
  switch((TCHAR)wParam) {
  case VK_UP:if (cursorat>=numcolumns) cursorat-=numcolumns; break;
  case VK_DOWN: 
    if (cursorat+numcolumns<=d) cursorat+=numcolumns; 
    else cursorat=d;
    break;
  case VK_RIGHT: if (cursorat<d) cursorat++; break;
  case VK_LEFT: if (cursorat>0) cursorat--; break;
  case VK_PRIOR: // page up
    if (cursorat >= numcolumns * numlines) {
      fvline = fvline >= numlines? fvline - numlines : 0;
      cursorat -= numcolumns * numlines;
      InvalidateRect(hwnd,NULL,1);
    } else cursorat %= numcolumns;
    break;
  case VK_NEXT: // page up
    if (cursorat + numcolumns * numlines <= d) {
      cursorat += numcolumns * numlines, fvline += numlines;
      InvalidateRect(hwnd,NULL,1);
    } else cursorat += (d - cursorat) / numcolumns * numcolumns;
    break;
  case VK_HOME:
    if (ctrl_down) cursorat = 0;
    else {
      if (cursorat && specialcursorpos) cursorat--;
      specialcursorpos = false;
      cursorat = cursorat / numcolumns * numcolumns;
    }
    break;
  case VK_END:
    if (ctrl_down) cursorat = d;
    else {
      if (!cursorat) specialcursorpos = false;
      else if (specialcursorpos) cursorat--;
      cursorat = (cursorat / numcolumns + 1) * numcolumns;
      if (cursorat > d) cursorat = d;
      else specialcursorpos = true;
    }
    break;

  case VK_BACK:
    if (e!=2) {
      if (cursorat==0) break;
      cursorat--;
    }
  case VK_DELETE: 
    if (readonly) break;
    if (isblockselected) he_DelSel();
    else he_DelByte();
    he_onchange(EN_CHANGE);
    need_at = false;
    break;
  case VK_APPS:
    EditMenu(0);
    return;//break;
  default:cursorinpos=(BYTE)e; need_at = false; break;
  }
  if (need_at) {
    if (shift_down) {
      if (!isblockselected) selectionstart = at;
      selectionend = cursorat;
      isblockselected = true;
      if (cursorat != at) InvalidateRect(hwnd,NULL,1);
    } else {
      if (isblockselected) InvalidateRect(hwnd,NULL,1);
      isblockselected = false;
    }
  }
  HEXEdit_SetCaretPos();
}

int he_CopyToClipboard(HWND hwnd, const char *data, int len) {
  if (!OpenClipboard(hwnd)) return 1;
  HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, len + 4);
  if (g) {
    char *c = (char*)GlobalLock(g);
    if (c) {
      *(DWORD*)c = len;
      memcpy(c + 4, data, len);
      GlobalUnlock(g);
      EmptyClipboard();
      SetClipboardData(hcf, g);
    }
  }
  CloseClipboard();
  return 0;
}

void HexEditorState::CutOrCopy(bool cut) {
  DWORD sels = min(selectionstart, selectionend), sele = max(selectionstart, selectionend);
  DWORD len = sele - sels;
  bool ok = len && !he_CopyToClipboard(hwnd, dataptr + sels, len);
  if (ok && cut && !readonly) { //^X
    he_DelSel();
    he_onchange(EN_CHANGE);
  }
}
void HexEditorState::Paste() {
  if (readonly || !OpenClipboard(0)) return;
  HANDLE h = GetClipboardData(hcf);
  if (!h) { CloseClipboard(); return; }
  BYTE *p = (BYTE*)GlobalLock(h); DWORD n = GlobalSize(h);
  if (p==0 || n<4 || (*(DWORD*)p>n-4)) { if (p) GlobalUnlock(h); CloseClipboard(); return; }
  cursorinpos=0;
  n=*(DWORD*)p;
  if (isblockselected) he_DelSel();
  he_InsBytes(p+4,n);
  cursorat += n;
  GlobalUnlock(h); CloseClipboard();
  he_onchange(EN_CHANGE);
}

void HexEditorState::onChar(WPARAM wParam) {
  TCHAR tc=(TCHAR)wParam;
  if (tc==9) {
    if (cursorinpos == 2) cursorat++;
    cursorinpos=(cursorinpos!=1);
    HEXEdit_SetCaretPos(true, true);
  } else if ((unsigned)tc>=32 && cursorinpos==1) {
    if (readonly) return;
    if (isblockselected) he_DelSel();
    he_InsByte(tc);
    cursorat++;
    he_onchange(EN_CHANGE);
  } else if ((tc>='0' && tc<='9') || (tc>='A' && tc<='F') || (tc>='a' && tc<='f')) {
    if (readonly) return;
    if (cursorinpos==0) {
      if (isblockselected) he_DelSel();
      he_InsByte(HexDigVal(tc)<<4);
      cursorinpos=2;
    } else {
      FixAdd1Action(dataptr[cursorat++] |= HexDigVal(tc));
      cursorinpos=0;
    }
    he_onchange(EN_CHANGE);
  } else if (tc=='\3' || tc=='\30') {//^C = copy || ^X = cut
    CutOrCopy(tc=='\30');
  } else if (tc=='\26') {//^V = paste
    Paste();
  } else if (tc=='\1') {//^A = select all
    selectionstart = 0, selectionend = datalen, isblockselected = true;
    InvalidateRect(hwnd,NULL,1);
  } else if (tc=='\32') {//^Z = undo
    Undo();
  } else if (tc=='\31') {//^Y = redo
    Redo();
  }
}

void HexEditorState::onKeyUp(WPARAM wParam) {
  if (wParam == VK_CONTROL && decimalflag) {
    if (isblockselected) he_DelSel();
    he_InsByte(decimalbyte);
    cursorat++;
    he_onchange(EN_CHANGE);
    decimalflag = false;
    InvalidateRect(hwnd, NULL, 1);
  }
}

void HexEditorState::EditMenu(LPARAM lParam) {
  HMENU pum = CreatePopupMenu();
  DWORD mp = GetMessagePos();
  int cnc = MF_GRAYED, pnc = MF_GRAYED, snc = MF_GRAYED, ro = readonly? MF_GRAYED : 0;
  if (selectionstart != selectionend && isblockselected) cnc = 0;
  if (datalen) snc = 0;
  if (!readonly && IsClipboardFormatAvailable(hcf)) pnc = 0;
  AppendMenu(pum, MF_STRING | cnc | ro, 1, _T("Cu&t\t^X"));
  AppendMenu(pum, MF_STRING | cnc, 2, _T("&Copy\t^C"));
  AppendMenu(pum, MF_STRING | pnc, 3, _T("&Paste\t^V"));
  AppendMenu(pum, MF_STRING | cnc | ro, 4, _T("&Delete\tDel"));
  AppendMenu(pum,MF_SEPARATOR,-1,_T(""));
  bool undo_setup = LOWORD(lParam) / he_charwidth < 6;
  if (undo_setup || !undo_disabled) {
    int unc = (undo_disabled || undopos == 0)? MF_GRAYED : 0;
    int rnc = (undo_disabled || undopos == undoact.size())? MF_GRAYED : 0;
    AppendMenu(pum, MF_STRING | unc | ro, 5, _T("&Undo\t^Z"));
    AppendMenu(pum, MF_STRING | rnc | ro, 6, _T("&Redo\t^Y"));
    if (undo_setup) {
      TCHAR buf[64];
      _stprintf(buf, _T("&Enable Undo&&Redo (%zu / %zu bytes)"), undobuf.size() + undoact.size() * sizeof(undo_struct), undobuf.capacity() + undoact.capacity() * sizeof(undo_struct));
      AppendMenu(pum, MF_STRING | ro | (undo_disabled? 0 : MF_CHECKED), 11, buf);
    }
    AppendMenu(pum,MF_SEPARATOR,-1,_T(""));
  }
  AppendMenu(pum, MF_STRING | snc, 10, _T("Select &All\t^A"));
  cnc = TrackPopupMenu(pum, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
			LOWORD(mp),HIWORD(mp), 0, hwnd, NULL);
  DestroyMenu(pum);
  switch(cnc) {
  case 1: CutOrCopy(true); break;
  case 2: CutOrCopy(false); break;
  case 3: Paste(); break;
  case 4: if (!readonly) { he_DelSel(); he_onchange(EN_CHANGE); } break;
  case 5: Undo(); break;
  case 6: Redo(); break;
  case 10: selectionstart = 0, selectionend = datalen, isblockselected = true;
    InvalidateRect(hwnd,NULL,1);
    break;
  case 11: EnableUndo(undo_disabled); break;
  }
}

LRESULT CALLBACK MyHexEditProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  HexEditorState *hs=(HexEditorState*)GetWindowLongPtr(hwnd, 8);
  char *c,*s; DWORD d,n; bool needrepaint;
  RECT rect;
  switch (msg) {
  case WM_CHAR:
    hs->onChar(wParam);
	break;
  case WM_KEYDOWN:
    hs->onKeyDown(wParam);
	break;
  case WM_KEYUP:
    hs->onKeyUp(wParam);
    break;

  case WM_CREATE:
	hs = new HexEditorState(hwnd);
	SetWindowLongPtr(hwnd, 8, (LONG_PTR)hs);//Editor state
	/*!*/if (hs->numcolumns<2) hs->numcolumns=2;
    GetClientRect(hwnd, &rect);
	//hs->scrollwnd = CreateWindow("SCROLLBAR","HexScroll",WS_CHILD | WS_VISIBLE | SBS_RIGHTALIGN | SBS_VERT, rect.right-16,0,16,rect.bottom,hwnd,(HMENU)1111,hInst,0);
    hs->setScrollRange();
	break;

  case WM_GETDLGCODE://stopped here
	if (lParam && ((LPMSG)lParam)->message==WM_KEYDOWN && 
		(TCHAR)((LPMSG)lParam)->wParam==9 &&
		hs->cursorinpos==1) {
	  hs->cursorinpos=0;
	  return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_HASSETSEL;
	}
	if (hs->cursorinpos!=1) return DLGC_WANTARROWS | DLGC_WANTTAB | DLGC_WANTCHARS | DLGC_HASSETSEL;
	else return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_HASSETSEL;

  case WM_SETFOCUS:
	CreateCaret(hwnd,NULL,0,12-1);
	hs->HEXEdit_SetCaretPos(true, true);
	ShowCaret(hwnd);
	break;
  case WM_KILLFOCUS:
	DestroyCaret();
	break;

  case WM_PAINT:
    hs->Paint();
	break;

  case WM_VSCROLL:
    return hs->vscroll(wParam);

  case WM_LBUTTONDOWN:
	n = hs->cursorat;
    hs->he_MouseAt(LOWORD(lParam),HIWORD(lParam), 0);
    needrepaint = false;
    if (hs->isblockselected) {
      if (wParam & MK_SHIFT) hs->selectionend = hs->cursorat, needrepaint = true;
      else hs->isblockselected = false, hs->selectionstart = hs->cursorat, needrepaint = true;
    } else {
      if (wParam & MK_SHIFT) {
        hs->isblockselected = (hs->selectionstart = n) != (hs->selectionend = hs->cursorat);
        needrepaint = true;
      } else hs->selectionstart = hs->cursorat;
    }
    if (needrepaint)
      InvalidateRect(hwnd,NULL,1);
    SetFocus(hwnd);
    SetCapture(hwnd);
	break;

  case WM_LBUTTONUP:
    ReleaseCapture();
    break;

  case WM_MOUSEMOVE:
    if (wParam & MK_LBUTTON) {
      bool prevs = hs->isblockselected; n = hs->selectionend;
      hs->he_MouseAt(LOWORD(lParam), HIWORD(lParam), 2);
      hs->isblockselected = hs->selectionstart != hs->selectionend;
      if (prevs != hs->isblockselected || n != hs->selectionend)
        InvalidateRect(hwnd,NULL,1);
    }
	break;

  case WM_MOUSEWHEEL:
    n = 1; //An ugly hack
    if (short(wParam>>16) < 0) n = hs->vscroll(SB_LINEDOWN);
    else if (short(wParam>>16) > 0) n = hs->vscroll(SB_LINEUP);
    if (n) return DefWindowProc(hwnd,msg,wParam,lParam);
    break;

  case WM_RBUTTONUP:
    hs->EditMenu(lParam);
    break;

  case WM_MBUTTONDOWN:
    SetFocus(hwnd);
    break;
  
  case WM_SIZE:
    //n=LOWORD(lParam), k=HIWORD(lParam); 
    //MoveWindow(GetDlgItem(hwnd, 1111), n-16,0,16,k, 0); 
    break;

  case WM_USER://set bytes
	c = hs->dataptr;
	if (c) free(c);
	s=(char*)wParam, d=lParam;
	if (!s) {
	  hs->dataptr = 0;
	  hs->datalen = 0;
	  return 0;
	}
	c=(char*)malloc(d+1); c[d]=0;
	memcpy(c,s,d);
	hs->dataptr = c;
	hs->datalen = d;
    hs->setScrollRange();
	return 0;
  case WM_USER+1://get length
	return hs->datalen;
  case WM_USER+2://get bytes. Return number of bytes needed
	d = hs->datalen;
	c = hs->dataptr;
	if (!c || !d) return 0;
	if ((DWORD)lParam<d) return d;
	memcpy((char*)wParam,c,d);
	return d;

  case WM_USER+3://Ensure that caret (text cursor) is visible
	break;

  case EM_SETREADONLY:
    hs->readonly = wParam != 0;
    break;
	
  case WM_DESTROY:
    SetWindowLongPtr(hwnd, 8, 0);
	delete hs;
	//break;
  default:return DefWindowProc(hwnd,msg,wParam,lParam);
  }
  return 0;
}


//++++ History in edit controls ++++++++++++++++++++++++++++++++++++++++++++++++++

WNDPROC savedOrigEditProc = 0, savedOrigListProc = 0;
HWND HistListParent = 0;
typedef map<HWND, int> hwnd2id_map;
hwnd2id_map hwnd2id;
typedef map<int, vector<TCHAR*> > id2hist_map;
id2hist_map id2hist;

LRESULT APIENTRY HistListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_KILLFOCUS) {
    SendMessage(hwnd, WM_CLOSE, 0, 0);
    return 0;
  }
  if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
    SendMessage(hwnd, WM_CLOSE, 0, 0);
    return 0;
  }
  if (msg == WM_KEYDOWN && wParam == VK_RETURN || msg == WM_LBUTTONDBLCLK) {
    //HWND parent = GetParent(hwnd);
    int currit = SendMessage(hwnd, LB_GETCURSEL, 0, 0);
    if (currit != LB_ERR) {
      int len = SendMessage(hwnd, LB_GETTEXTLEN, currit, 0);
      achar currtxt; currtxt.resize(len);
      SendMessage(hwnd, LB_GETTEXT, currit, (LPARAM)currtxt.c);
      currtxt.checklen();
      SetWindowText(HistListParent, currtxt.c);
      SendMessage(HistListParent, EM_SETSEL, currtxt.l, currtxt.l);
    }
    SendMessage(hwnd, WM_CLOSE, 0, 0);
    return 0;
  }
  return CallWindowProc(savedOrigListProc, hwnd, msg, wParam, lParam);
}

LRESULT APIENTRY HistEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) { 
  if ( msg == WM_KEYDOWN && (wParam == VK_UP || wParam == VK_DOWN)
    || msg == WM_LBUTTONDBLCLK && !SendMessage(hwnd, WM_GETTEXTLENGTH,0,0)) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    HWND lbw = CreateWindowEx(WS_EX_TOOLWINDOW, _T("LISTBOX"), _T("history"), WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_POPUP | WS_VISIBLE, rect.left, rect.top + 20, 
      250, 150, hwnd, 0, hInst, 0);
    savedOrigListProc = (WNDPROC)SetWindowLongPtr(lbw, GWLP_WNDPROC, (LONG_PTR)HistListProc);
    SetFocus(lbw);
    HistListParent = hwnd;
    vector<TCHAR*> &v = id2hist[hwnd2id[hwnd]];
    if (!v.empty()) {
      for(vector<TCHAR*>::iterator c = v.end(); c != v.begin(); ) {
        SendMessage(lbw, LB_ADDSTRING, 0, (LPARAM)*--c);
      }
      SendMessage(lbw, LB_SETCURSEL, 0, 0);
    }
    return 0;
  }
#ifndef _WIN32_WCE
  if (msg == WM_DROPFILES) { // a dirty hack (I'm just too lazy)
    UINT l = DragQueryFile((HDROP)wParam, 0, 0, 0);
    fchar fname(malloc(l + 1));
    DragQueryFile((HDROP)wParam, 0, fname.c, l + 1);
    DragFinish((HDROP)wParam);
    SetWindowText(hwnd, fname.c);
    return 0;
  }
#endif
  if (msg == WM_DESTROY) {
    vector<TCHAR*> &v = id2hist[hwnd2id[hwnd]];
    DWORD L = SendMessage(hwnd, WM_GETTEXTLENGTH,0,0);
    TCHAR *t = (TCHAR*)malloc((L + 2) * sizeof(TCHAR));
    GetWindowText(hwnd, t, L + 1);
    if (*t) {
      for(vector<TCHAR*>::iterator c = v.begin(); c != v.end(); c++) {
        if (!_tcscmp(*c, t)) { 
          free(t); 
          t = v.back(); v.back() = *c; *c = t;
          t = 0; 
          break;
        }
      }
      if (t) v.push_back(t);
    } else {
      free(t);
    }
    hwnd2id.erase(hwnd);
  }
  return CallWindowProc(savedOrigEditProc, hwnd, msg, wParam, lParam); 
} 


int SetupEditControlHistory(HWND hwnd, int histid) {
  WNDPROC OrigEditProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)HistEditProc);
  hwnd2id[hwnd] = histid;
  if (savedOrigEditProc == 0) savedOrigEditProc = OrigEditProc;
  else if (savedOrigEditProc != OrigEditProc) {
    MessageBox(hwnd, _T("Improper use of SetupEditControHistory"), _T("Internal error!"), MB_ICONERROR);
    return 1;
  }
  return 0;
}

int SetDlgEditCtrlHist(HWND hwnd, int item) {
  return SetupEditControlHistory(GetDlgItem(hwnd, item), item);
}
int SetEditCtrlHistAndText(HWND hwnd, int item, const TCHAR *c) {
  SetupEditControlHistory(GetDlgItem(hwnd, item), item);
  if (!c) return 0;
  return SetDlgItemText(hwnd, item, c);
}

//++++ My personal TreeWiew (?) ++++++++++++++++++++++++++++++++++++++++++++++++++

enum TreeSortTypes {
    SORT_ALNUM = 0
};

struct TreeLeave;
struct TreeLeaveInfo {
    char *name;
    int has_subtree /*a hint to draw '[+]'*/,
        expanded_total /*number of lines spawned by expanded subtree(s); 0 - not expanded */;
    int subtree_size, subtree_asize;
    TreeLeave *owner, *subtree;
    TreeLeaveInfo *parent;
    //Some custom fields will be here --
    //----------------------------------
    TreeLeaveInfo(TreeLeave *ow, TreeLeaveInfo *par, int hst = 0) : name(0)
        , has_subtree(hst), expanded_total(0)
        , subtree_size(0), subtree_asize(0)
        , owner(ow), subtree(0), parent(par) {}
    ~TreeLeaveInfo();
    void reserve(int n);
    void add(TreeLeave &);

    void sort_subtree(TreeSortTypes st, int recurs = 0);
    int expand();
    int expand_parents();
    int shrink();
private:
    TreeLeaveInfo(const TreeLeaveInfo &); //kill
    void operator =(const TreeLeaveInfo &); //kill
};

//this structure is basically used to sort leaves
struct TreeLeave {
    TreeLeaveInfo *data;
    TreeLeave() : data(0) {}
    void relink() { data->owner = this; }
    const TreeLeave &operator =(const TreeLeave &src) {
        data = src.data;
        relink();
        return *this;
    }
    TreeLeaveInfo *init(TreeLeaveInfo *par, int hst = 0) {
        return data = new TreeLeaveInfo(this, par, hst);
    }
    void destroy();
    bool operator<(const TreeLeave& t) const { return _stricmp(data->name, t.data->name) < 0; }

    int getIndex() {
        if (!data->parent) return -1;
        return (this - data->parent->subtree) / sizeof(*this);
    }
};

void TreeLeaveInfo::sort_subtree(TreeSortTypes st, int recurs) {
    switch(st) {
    case SORT_ALNUM: 
        sort(subtree, subtree + subtree_size);
        break;
    default: return;
    }
    if (recurs != 0) for(int n = 0; n < subtree_size; n++) {
        subtree[n].data->sort_subtree(st, recurs - 1);
    }
}

int TreeLeaveInfo::expand() {
    if (expanded_total) return expanded_total;
    int esz = subtree_size;
    for(int n = 0; n < subtree_size; n++) {
        esz += subtree[n].data->expanded_total;
    }
    //Update expanded_total for all expanded parents
    for(TreeLeaveInfo *p = parent; p && p->expanded_total; p = p->parent) {
        p->expanded_total += esz;
    }
    return esz;
}
int TreeLeaveInfo::expand_parents() {
    //this function may sometimes work in a sqare time, but it's not too crtical
    int esz = 0;
    for(TreeLeaveInfo *p = parent; p; p = p->parent) {
        if (!p->expanded_total)
            esz = p->expand();
    }
    return esz;
}
int TreeLeaveInfo::shrink() {
    if (!expanded_total) return 0;
    int esz = expanded_total;
    expanded_total = 0;
    //Update expanded_total for all expanded parents
    for(TreeLeaveInfo *p = parent; p && p->expanded_total; p = p->parent) {
        p->expanded_total -= esz;
        assert(p->expanded_total > 0);
    }
    return esz;
}

inline TreeLeaveInfo::~TreeLeaveInfo() {
    for(int n = 0; n < subtree_size; n++) {
        subtree[n].destroy();
    }
    delete subtree;
}
void TreeLeave::destroy() {
    delete data; data = 0;
}

struct TreeVInf {
    TreeLeave root;
    TreeLeaveInfo *cursel;
    int curselatline, curline;
    TreeVInf() {
        root.init(0);
        cursel = root.data;
        curselatline = 0;
    }
    void Redraw(HDC hdc) {
    }
};
