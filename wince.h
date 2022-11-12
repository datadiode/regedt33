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
#if (_WIN32_WCE == 0x800) && defined(_X86_)
// To compensate for Compact2013_SDK_86Duino_80B's lack of CE_MODULES_COMMCTRL:
// - Use Header from <../../../../wce600/Beckhoff_HMI_600/Include/X86/commctrl.h>
// - Use ImpLib from $(SdkRootPath)..\..\..\wce600\Beckhoff_HMI_600\Lib\x86\commctrl.lib
#pragma include_alias(<commctrl.h>,<../../../../wce600/Beckhoff_HMI_600/Include/X86/commctrl.h>)
#endif
#else
#define GetProcAddressA GetProcAddress
#endif
