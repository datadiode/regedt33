#include "regedit.h"
#include "resource.h"

extern HBITMAP img_up,img_down;

INT_PTR CALLBACK EditString (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  val_ed_dialog_data *params;
  switch (msg) {
  case WM_INITDIALOG: 
    SetWindowLongPtr(hwnd, DWLP_USER, lParam);
	params = (val_ed_dialog_data*)lParam;
	if (params->readonly) {
	  SendDlgItemMessage(hwnd,IDC_EDITSTR,EM_SETREADONLY,1,0);
	  EnableWindow(GetDlgItem(hwnd,IDOK),0);
	  SetWindowText(hwnd,_T("Edit String (read only)"));
	}
	params->applyEn = true;
	SetDlgItemText(hwnd,IDC_VNAME,params->name.c);
	SetDlgItemText(hwnd,IDC_EDITSTR,params->data.c);
	params->applyEn = false;
	SetFocus(GetDlgItem(hwnd,IDC_EDITSTR));
	SendDlgItemMessage(hwnd,IDC_EDITSTR,EM_SETSEL,0,-1);
	return 0;
  case WM_CLOSE: EndDialog(hwnd,0); return 1;
  case WM_COMMAND:
    params = (val_ed_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
	switch (LOWORD (wParam)) {
	case IDC_APPLY:
	  params->applyEn=0;
	  EnableWindow(GetDlgItem(hwnd,IDC_APPLY),0);
	  SetFocus(GetDlgItem(hwnd,IDC_EDITSTR));
	case IDOK: 
	  params->newdata.GetDlgItemText(hwnd,IDC_EDITSTR);
	  if (LOWORD(wParam)==IDOK) EndDialog(hwnd,1);
	  break;
	case IDCANCEL: EndDialog(hwnd,0); break; 

	case IDC_EDITSTR:
	  if (HIWORD(wParam)==EN_CHANGE && !params->applyEn) {
		params->applyEn = 1;
		EnableWindow(GetDlgItem(hwnd,IDC_APPLY),1);
	  }
	  break;
	}
	return 1;
  }
  return 0;
}

#define DISABLE_LIST_DRAG
INT_PTR CALLBACK EditMString (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  TCHAR *c;
  int k,l,m,n;
  TCHAR s[64];
  val_ed_dialog_data *params;
  static DWORD numstr;
  static int cursel;
  static UINT WM_DRAG_LIST;
  switch (msg) {
  case WM_INITDIALOG: 
    SetWindowLongPtr(hwnd, DWLP_USER, lParam);
	params = (val_ed_dialog_data*)lParam;
	cursel=0;
	if (params->readonly) {
	  SendDlgItemMessage(hwnd,IDC_EDITSTR,EM_SETREADONLY,1,0);
	  EnableWindow(GetDlgItem(hwnd,IDOK),0);
	  EnableWindow(GetDlgItem(hwnd,IDC_MOVEUP),0);
	  EnableWindow(GetDlgItem(hwnd,IDC_MOVEDOWN),0);
	  EnableWindow(GetDlgItem(hwnd,IDC_NEW),0);
	  EnableWindow(GetDlgItem(hwnd,IDC_DEL),0);
	  EnableWindow(GetDlgItem(hwnd,IDC_CONCAT),0);
	  SetWindowText(hwnd,_T("Edit Multistring (read only)"));
	}
#ifndef DISABLE_LIST_DRAG
    MakeDragList(GetDlgItem(hwnd, IDC_SLIST));
    WM_DRAG_LIST = RegisterWindowMessage(DRAGLISTMSGSTRING);
#endif
	params->applyEn = 2;
#ifndef _WIN32_WCE
	SendDlgItemMessage(hwnd,IDC_MOVEUP,BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)img_up);
	SendDlgItemMessage(hwnd,IDC_MOVEDOWN,BM_SETIMAGE,IMAGE_BITMAP,(LPARAM)img_down);
#endif
	SetDlgItemText(hwnd,IDC_VNAME,params->name.c);
	SetDlgItemText(hwnd,IDC_EDITSTR,params->data.c);
	for(n=numstr=0,c=params->data.c;n<(int)params->data.l /*&& c[n]*/;) {
	  k=_tcslen(c+n);
	  if (k==0 && n==(int)params->data.l-1) break;
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_ADDSTRING,0,(LPARAM)c+n);
	  numstr++;
	  n+=k+1;
	}
	SendDlgItemMessage(hwnd,IDC_SLIST,LB_SETCURSEL,0,0);
	params->applyEn = 0;
	SetFocus(GetDlgItem(hwnd,IDC_EDITSTR));
	SendDlgItemMessage(hwnd,IDC_EDITSTR,EM_SETSEL,0,-1);
	_stprintf(s,_T("String %i of %i:"),cursel,
		(int)SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCOUNT,0,0));
	SetDlgItemText(hwnd,IDC_VSLIST,s);
	return 0;
  case WM_CLOSE: EndDialog(hwnd,0); return 1;
  case WM_COMMAND:
    params = (val_ed_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
	switch (LOWORD (wParam)) {
	case IDC_APPLY:
	  params->applyEn = 0;
	  EnableWindow(GetDlgItem(hwnd,IDC_APPLY),0);
	  SetFocus(GetDlgItem(hwnd,IDC_EDITSTR));
	case IDOK: 
	  l=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCOUNT,0,0);
	  for(n=k=0;n<l;n++) {
		m=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXTLEN,n,0);
		if (m!=LB_ERR) k+=m+1; else k+=30;
	  }
      c = params->newdata.resize(k + 4);
	  for(n=m=0;n<l;n++) {
		c[m]=0;
		SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXT,n,(LPARAM)(c+m));
		m+=_tcslen(c+m)+1;
	  }
	  c[m++]=0;
	  params->newdata.l = m;
	  if (LOWORD(wParam)==IDOK) EndDialog(hwnd,1);
	  break;
	case IDCANCEL: EndDialog(hwnd,0); break; 

	case IDC_EDITSTR:
	  if (HIWORD(wParam)==EN_CHANGE) {
		if (!params->applyEn) {
		  params->applyEn = 1;
		  EnableWindow(GetDlgItem(hwnd,IDC_APPLY),1);
		}
		if (params->applyEn != 2) {
		  DWORD l=SendDlgItemMessage(hwnd,IDC_EDITSTR,WM_GETTEXTLENGTH,0,0);
		  c=(TCHAR*)malloc((l+10) * sizeof(TCHAR));
		  GetDlgItemText(hwnd,IDC_EDITSTR,c,l+9);
		  SendDlgItemMessage(hwnd,IDC_SLIST,LB_DELETESTRING,cursel,0);
		  SendDlgItemMessage(hwnd,IDC_SLIST,LB_INSERTSTRING,cursel,(LPARAM)c);
		  SendDlgItemMessage(hwnd,IDC_SLIST,LB_SETCURSEL,cursel,0);
		  free(c);
		  _stprintf(s,_T("String %i of %i:"),cursel,
			  (int)SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCOUNT,0,0));
		  SetDlgItemText(hwnd,IDC_VSLIST,s);
		}
	  }
	  break;
	case IDC_SLIST:
	  if (HIWORD(wParam)==LBN_SELCHANGE) {
setsel:	cursel=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCURSEL,0,0);
		if (cursel<0) cursel=0; else {
		  DWORD l1=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXTLEN,cursel,0),
			l=SendDlgItemMessage(hwnd,IDC_EDITSTR,WM_GETTEXTLENGTH,0,0);
		  TCHAR *c1;
		  c=(TCHAR*)malloc((l+10) * sizeof(TCHAR)); c1=(TCHAR*)malloc((l1+10) * sizeof(TCHAR));
		  *c=*c1=0;
		  GetDlgItemText(hwnd,IDC_EDITSTR,c,l+9);
		  SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXT,cursel,(LPARAM)c1);
		  int t = params->applyEn;
		  params->applyEn = 2;
		  if (_tcscmp(c,c1)) SetDlgItemText(hwnd,IDC_EDITSTR,c1);
		  params->applyEn = t;
		  free(c); free(c1);
		}
		_stprintf(s,_T("String %i of %i:"),cursel,
			(int)SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCOUNT,0,0));
		SetDlgItemText(hwnd,IDC_VSLIST,s);
	  }
	  break;
	  
	case IDC_NEW:
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_INSERTSTRING,cursel,(LPARAM)"");
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_SETCURSEL,cursel,0);
	  goto setsel;
	  break;

	case IDC_DEL:
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_DELETESTRING,cursel,0);
	  l=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCOUNT,0,0);
	  cursel=(cursel>=l)?(l>0)?l-1:0:cursel;
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_SETCURSEL,cursel,0);
	  goto setsel;
	  break;
	
	case IDC_MOVEUP:
	  if (cursel<=0) {cursel=0; break;}
	  m=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXTLEN,cursel,0);
	  c=(TCHAR*)malloc((m+5) * sizeof(TCHAR));
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXT,cursel,(LPARAM)c);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_DELETESTRING,cursel,0);
	  cursel--;
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_INSERTSTRING,cursel,(LPARAM)c);
	  free(c);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_SETCURSEL,cursel,0);
	  goto setsel;
	  break;
	case IDC_MOVEDOWN:
	  l=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCOUNT,0,0);
	  if (cursel>=l-1) break;
	  m=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXTLEN,cursel,0);
	  c=(TCHAR*)malloc((m+5) * sizeof(TCHAR));
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXT,cursel,(LPARAM)c);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_DELETESTRING,cursel,0);
	  cursel++;
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_INSERTSTRING,cursel,(LPARAM)c);
	  free(c);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_SETCURSEL,cursel,0);
	  goto setsel;
	  break;

	case IDC_CONCAT:
	  l=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETCOUNT,0,0);
	  if (cursel>=l-1) break;
	  l=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXTLEN,cursel,0);
	  m=SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXTLEN,cursel+1,0);
	  c=(TCHAR*)malloc((l+m+5) * sizeof(TCHAR)); c[0]=0;
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXT,cursel,(LPARAM)c);
	  l=_tcslen(c);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_GETTEXT,cursel+1,(LPARAM)(c+l));
	  c[l+m]=0;
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_DELETESTRING,cursel+1,0);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_DELETESTRING,cursel,0);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_INSERTSTRING,cursel,(LPARAM)c);
	  free(c);
	  SendDlgItemMessage(hwnd,IDC_SLIST,LB_SETCURSEL,cursel,0);
	  goto setsel;
	  break;

	}
	return 1;
  }
#ifndef DISABLE_LIST_DRAG
  if (msg == WM_DRAG_LIST) {
    int lbitem; HWND lbwnd = GetDlgItem(hwnd, IDC_SLIST);
    DRAGLISTINFO &dli = *(DRAGLISTINFO*)lParam; //don't need wParam

    static int drlcount = 0;
    char s[64]; sprintf(s, "WM_DRAG_LIST [%04i]: ntfn = %i\n", drlcount++, dli.uNotification);
    SetWindowText(hwnd, s);

    switch(dli.uNotification) {
    case DL_BEGINDRAG: 
      return true;
    case DL_DROPPED: 
      // move item here
      return 0; //don't care
    case DL_CANCELDRAG: return 0; //don't care
    case DL_DRAGGING: //Where's the message ?! :(
      lbitem = LBItemFromPt(lbwnd, dli.ptCursor, true);
      DrawInsert(hwnd, lbwnd, lbitem);
      return DL_MOVECURSOR;
    }
  }
#endif
  return 0;
}

INT_PTR CALLBACK EditDWORD (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  static int edval,cval,lock;
  val_ed_dialog_data *params;
  TCHAR s[64];
  int n;
  switch (msg) {
  case WM_INITDIALOG: 
    SetWindowLongPtr(hwnd, DWLP_USER, lParam);
	params = (val_ed_dialog_data*)lParam;
	*((DWORD*)params->newdata.c) = cval = *((DWORD*)params->data.c);
	if (params->data.l != 4) SetWindowText(hwnd, _T("Edit false DWORD Value?!"));
	SetDlgItemText(hwnd,IDC_VNAME,params->name.c);
	if (params->readonly) {
	  SendDlgItemMessage(hwnd,IDC_DECNUM,EM_SETREADONLY,1,0);
	  SendDlgItemMessage(hwnd,IDC_HEXNUM,EM_SETREADONLY,1,0);
	  EnableWindow(GetDlgItem(hwnd,IDOK),0);
	}
	lock = 1,params->applyEn = 2,edval = 0;
	_stprintf(s,_T("%i"),cval);
	SetDlgItemText(hwnd,IDC_DECNUM,s);
	_stprintf(s,_T("%X"),cval);
	SetDlgItemText(hwnd,IDC_HEXNUM,s);
	_stprintf(s,_T("0x%08X (%i)"),cval,cval);
	SetDlgItemText(hwnd,IDC_RESULTNUM,s);
	SendDlgItemMessage(hwnd,IDC_DECNUM,EM_SETLIMITTEXT,32,0);
	SendDlgItemMessage(hwnd,IDC_HEXNUM,EM_SETLIMITTEXT,32,0);
	lock = params->applyEn = 0;
	return 0;
  case WM_CLOSE: EndDialog(hwnd,edval); return 1;
  case WM_COMMAND:
    params = (val_ed_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
	switch (LOWORD (wParam)) {
	case IDOK: 
	  *((DWORD*)params->newdata.c) = cval;
	  EndDialog(hwnd,1);
	  break;
	case IDCANCEL: EndDialog(hwnd,edval); break; 
	case IDC_APPLY: 
	  *((DWORD*)params->newdata.c) = cval; 
	  params->applyEn = 0, edval = 1;
	  EnableWindow(GetDlgItem(hwnd,IDC_APPLY),0);
	  break;

	case IDC_DECNUM:
	  if (HIWORD(wParam)==EN_UPDATE && !lock) {
		GetDlgItemText(hwnd,IDC_DECNUM,s,64);
		_stscanf(s,_T("%i"),&cval);
		_stprintf(s,_T("%X"),cval);
		lock=1;
		SetDlgItemText(hwnd,IDC_HEXNUM,s);
		_stprintf(s,_T("0x%08X (%i)"),cval,cval);
		SetDlgItemText(hwnd,IDC_RESULTNUM,s);
		lock=0;
		if (!params->applyEn) {
		  params->applyEn = 1;
		  EnableWindow(GetDlgItem(hwnd,IDC_APPLY),1);
		}
	  }
	  break;
	case IDC_HEXNUM:
	  if (HIWORD(wParam)==EN_UPDATE && !lock) {
		GetDlgItemText(hwnd,IDC_HEXNUM,s,64);
		for(n=0,cval=0;isxdigit((unsigned char)s[n]);n++) 
          cval=(cval<<4)| ((s[n]<='9')? s[n]-'0' : toupper(s[n])-'A'+10);
		_stprintf(s,_T("%i"),cval);
		lock=1;
		SetDlgItemText(hwnd,IDC_DECNUM,s);
		_stprintf(s,_T("0x%08X (%i)"),cval,cval);
		SetDlgItemText(hwnd,IDC_RESULTNUM,s);
		lock=0;
		if (!params->applyEn) {
		  params->applyEn = 1;
		  EnableWindow(GetDlgItem(hwnd,IDC_APPLY),1);
		}
	  }
	  break;
	}
	return 1;
  }
  return 0;
}

INT_PTR CALLBACK EditBinary (HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam) {
  val_ed_dialog_data *params;
  TCHAR s[64];
  switch (msg) {
  case WM_INITDIALOG: 
    SetWindowLongPtr(hwnd, DWLP_USER, lParam);
	params = (val_ed_dialog_data*)lParam;
	if (params->readonly) {
	  SendDlgItemMessage(hwnd,IDC_HEXEDIT,EM_SETREADONLY,1,0);
	  EnableWindow(GetDlgItem(hwnd,IDOK),0);
	}
	_stprintf(s,_T("Edit %sBinary %s"), params->flag1?_T("value as "):_T(""),params->readonly?_T(" (read only)"):_T(""));
	SetWindowText(hwnd,s);
	params->applyEn = 1;
	SetDlgItemText(hwnd,IDC_VNAME,params->name.c);
	SendDlgItemMessage(hwnd,IDC_HEXEDIT,WM_USER,(WPARAM)params->data.c, params->data.l);
	params->applyEn = 0;
	SetFocus(GetDlgItem(hwnd,IDC_HEXEDIT));
	return 0;
  case WM_CLOSE: EndDialog(hwnd,0); return 1;
  case WM_COMMAND:
    params = (val_ed_dialog_data*)GetWindowLongPtr(hwnd, DWLP_USER);
	switch (LOWORD (wParam)) {
	case IDC_APPLY:
	  params->applyEn = 0;
	  EnableWindow(GetDlgItem(hwnd,IDC_APPLY),0);
	  SetFocus(GetDlgItem(hwnd,IDC_HEXEDIT));
	case IDOK: 
	  params->newdata.l = SendDlgItemMessage(hwnd,IDC_HEXEDIT,WM_USER+1,0,0);
	  params->newdata.resize();
      params->newdata.c[params->newdata.l] = 0;
	  SendDlgItemMessage(hwnd,IDC_HEXEDIT,WM_USER+2,(WPARAM)params->newdata.c,params->newdata.s);
	  if (LOWORD(wParam)==IDOK) EndDialog(hwnd,1);
	  break;
	case IDCANCEL: EndDialog(hwnd,0); break; 

	case IDC_HEXEDIT:
	  if (HIWORD(wParam)==EN_CHANGE && !params->applyEn) {
		params->applyEn = 1;
		EnableWindow(GetDlgItem(hwnd,IDC_APPLY),1);
	  }
	  break;
	}
	return 1;
  }
  return 0;
}
