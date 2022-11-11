/* _WIN32_WCE bypasses 64bit mode and lacks other stuff */
#ifdef _WIN32_WCE
#include <excpt.h>
#include <windef.h>
#define DWLP_USER DWL_USER
#define DWLP_MSGRESULT DWL_MSGRESULT
#define GWLP_WNDPROC GWL_WNDPROC
#define ULONG_PTR ULONG
#define LONG_PTR LONG
#define SetWindowLongPtr SetWindowLong
#define GetWindowLongPtr GetWindowLong
#define GetMessageTime GetTickCount
#define TPM_RIGHTBUTTON 0
#define WS_OVERLAPPEDWINDOW (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define SBARS_SIZEGRIP 0
#define WC_DIALOG L"Dialog"
#endif
