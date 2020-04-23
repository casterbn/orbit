//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------

#include "Utils.h"

#ifdef _WIN32
// clang-format off
#include <cguid.h>
#include <AtlBase.h>
#include <atlconv.h>
// clang-format on
#else
#include <sys/uio.h>
#endif

#include <time.h>
#include <algorithm>
#include <mutex>
#include <thread>

#include "absl/base/attributes.h"

#ifdef _WIN32
//-----------------------------------------------------------------------------
std::string GetLastErrorAsString() {
  // Get the error message, if any.
  DWORD errorMessageID = ::GetLastError();
  if (errorMessageID == 0) return "No error message has been recorded";

  LPSTR messageBuffer = nullptr;
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&messageBuffer, 0, NULL);

  std::string message(messageBuffer, size);

  // Free the buffer.
  LocalFree(messageBuffer);

  return message;
}

//-----------------------------------------------------------------------------
std::string GuidToString(GUID a_Guid) {
  std::string guidStr;
  LPOLESTR wszCLSID = NULL;
  HRESULT hr = StringFromCLSID(a_Guid, &wszCLSID);
  if (hr == S_OK) {
    // wszCLSID looks like: "{96F93FED-50B7-44B8-94AF-C205B68334FE}"
    guidStr = ws2s(wszCLSID);
    CoTaskMemFree(wszCLSID);
    auto len = guidStr.size();
    if (len > 2) {
      guidStr = guidStr.substr(1, len - 2);
    }
    guidStr.erase(std::remove(guidStr.begin(), guidStr.end(), '-'),
                  guidStr.end());
  }

  return guidStr;
}

//-----------------------------------------------------------------------------
std::wstring GuidToStringW(GUID a_Guid) { return s2ws(GuidToString(a_Guid)); }

#include "dde.h"

struct AFX_MAP_MESSAGE {
  UINT nMsg;
  LPCSTR lpszMsg;
};
#define DEFINE_MESSAGE(wm) \
  { wm, #wm }
static const AFX_MAP_MESSAGE allMessages[] = {
    DEFINE_MESSAGE(WM_CREATE),
    DEFINE_MESSAGE(WM_DESTROY),
    DEFINE_MESSAGE(WM_MOVE),
    DEFINE_MESSAGE(WM_SIZE),
    DEFINE_MESSAGE(WM_ACTIVATE),
    DEFINE_MESSAGE(WM_SETFOCUS),
    DEFINE_MESSAGE(WM_KILLFOCUS),
    DEFINE_MESSAGE(WM_ENABLE),
    DEFINE_MESSAGE(WM_SETREDRAW),
    DEFINE_MESSAGE(WM_SETTEXT),
    DEFINE_MESSAGE(WM_GETTEXT),
    DEFINE_MESSAGE(WM_GETTEXTLENGTH),
    DEFINE_MESSAGE(WM_PAINT),
    DEFINE_MESSAGE(WM_CLOSE),
    DEFINE_MESSAGE(WM_QUERYENDSESSION),
    DEFINE_MESSAGE(WM_QUIT),
    DEFINE_MESSAGE(WM_QUERYOPEN),
    DEFINE_MESSAGE(WM_ERASEBKGND),
    DEFINE_MESSAGE(WM_SYSCOLORCHANGE),
    DEFINE_MESSAGE(WM_ENDSESSION),
    DEFINE_MESSAGE(WM_SHOWWINDOW),
    DEFINE_MESSAGE(WM_CTLCOLORMSGBOX),
    DEFINE_MESSAGE(WM_CTLCOLOREDIT),
    DEFINE_MESSAGE(WM_CTLCOLORLISTBOX),
    DEFINE_MESSAGE(WM_CTLCOLORBTN),
    DEFINE_MESSAGE(WM_CTLCOLORDLG),
    DEFINE_MESSAGE(WM_CTLCOLORSCROLLBAR),
    DEFINE_MESSAGE(WM_CTLCOLORSTATIC),
    DEFINE_MESSAGE(WM_WININICHANGE),
    DEFINE_MESSAGE(WM_SETTINGCHANGE),
    DEFINE_MESSAGE(WM_DEVMODECHANGE),
    DEFINE_MESSAGE(WM_ACTIVATEAPP),
    DEFINE_MESSAGE(WM_FONTCHANGE),
    DEFINE_MESSAGE(WM_TIMECHANGE),
    DEFINE_MESSAGE(WM_CANCELMODE),
    DEFINE_MESSAGE(WM_SETCURSOR),
    DEFINE_MESSAGE(WM_MOUSEACTIVATE),
    DEFINE_MESSAGE(WM_CHILDACTIVATE),
    DEFINE_MESSAGE(WM_QUEUESYNC),
    DEFINE_MESSAGE(WM_GETMINMAXINFO),
    DEFINE_MESSAGE(WM_ICONERASEBKGND),
    DEFINE_MESSAGE(WM_NEXTDLGCTL),
    DEFINE_MESSAGE(WM_SPOOLERSTATUS),
    DEFINE_MESSAGE(WM_DRAWITEM),
    DEFINE_MESSAGE(WM_MEASUREITEM),
    DEFINE_MESSAGE(WM_DELETEITEM),
    DEFINE_MESSAGE(WM_VKEYTOITEM),
    DEFINE_MESSAGE(WM_CHARTOITEM),
    DEFINE_MESSAGE(WM_SETFONT),
    DEFINE_MESSAGE(WM_GETFONT),
    DEFINE_MESSAGE(WM_QUERYDRAGICON),
    DEFINE_MESSAGE(WM_COMPAREITEM),
    DEFINE_MESSAGE(WM_COMPACTING),
    DEFINE_MESSAGE(WM_NCCREATE),
    DEFINE_MESSAGE(WM_NCDESTROY),
    DEFINE_MESSAGE(WM_NCCALCSIZE),
    DEFINE_MESSAGE(WM_NCHITTEST),
    DEFINE_MESSAGE(WM_NCPAINT),
    DEFINE_MESSAGE(WM_NCACTIVATE),
    DEFINE_MESSAGE(WM_GETDLGCODE),
    DEFINE_MESSAGE(WM_NCMOUSEMOVE),
    DEFINE_MESSAGE(WM_NCLBUTTONDOWN),
    DEFINE_MESSAGE(WM_NCLBUTTONUP),
    DEFINE_MESSAGE(WM_NCLBUTTONDBLCLK),
    DEFINE_MESSAGE(WM_NCRBUTTONDOWN),
    DEFINE_MESSAGE(WM_NCRBUTTONUP),
    DEFINE_MESSAGE(WM_NCRBUTTONDBLCLK),
    DEFINE_MESSAGE(WM_NCMBUTTONDOWN),
    DEFINE_MESSAGE(WM_NCMBUTTONUP),
    DEFINE_MESSAGE(WM_NCMBUTTONDBLCLK),
    DEFINE_MESSAGE(WM_KEYDOWN),
    DEFINE_MESSAGE(WM_KEYUP),
    DEFINE_MESSAGE(WM_CHAR),
    DEFINE_MESSAGE(WM_DEADCHAR),
    DEFINE_MESSAGE(WM_SYSKEYDOWN),
    DEFINE_MESSAGE(WM_SYSKEYUP),
    DEFINE_MESSAGE(WM_SYSCHAR),
    DEFINE_MESSAGE(WM_SYSDEADCHAR),
    DEFINE_MESSAGE(WM_KEYLAST),
    DEFINE_MESSAGE(WM_INITDIALOG),
    DEFINE_MESSAGE(WM_COMMAND),
    DEFINE_MESSAGE(WM_SYSCOMMAND),
    DEFINE_MESSAGE(WM_TIMER),
    DEFINE_MESSAGE(WM_HSCROLL),
    DEFINE_MESSAGE(WM_VSCROLL),
    DEFINE_MESSAGE(WM_INITMENU),
    DEFINE_MESSAGE(WM_INITMENUPOPUP),
    DEFINE_MESSAGE(WM_MENUSELECT),
    DEFINE_MESSAGE(WM_MENUCHAR),
    DEFINE_MESSAGE(WM_ENTERIDLE),
    DEFINE_MESSAGE(WM_MOUSEWHEEL),
    DEFINE_MESSAGE(WM_MOUSEMOVE),
    DEFINE_MESSAGE(WM_LBUTTONDOWN),
    DEFINE_MESSAGE(WM_LBUTTONUP),
    DEFINE_MESSAGE(WM_LBUTTONDBLCLK),
    DEFINE_MESSAGE(WM_RBUTTONDOWN),
    DEFINE_MESSAGE(WM_RBUTTONUP),
    DEFINE_MESSAGE(WM_RBUTTONDBLCLK),
    DEFINE_MESSAGE(WM_MBUTTONDOWN),
    DEFINE_MESSAGE(WM_MBUTTONUP),
    DEFINE_MESSAGE(WM_MBUTTONDBLCLK),
    DEFINE_MESSAGE(WM_PARENTNOTIFY),
    DEFINE_MESSAGE(WM_MDICREATE),
    DEFINE_MESSAGE(WM_MDIDESTROY),
    DEFINE_MESSAGE(WM_MDIACTIVATE),
    DEFINE_MESSAGE(WM_MDIRESTORE),
    DEFINE_MESSAGE(WM_MDINEXT),
    DEFINE_MESSAGE(WM_MDIMAXIMIZE),
    DEFINE_MESSAGE(WM_MDITILE),
    DEFINE_MESSAGE(WM_MDICASCADE),
    DEFINE_MESSAGE(WM_MDIICONARRANGE),
    DEFINE_MESSAGE(WM_MDIGETACTIVE),
    DEFINE_MESSAGE(WM_MDISETMENU),
    DEFINE_MESSAGE(WM_CUT),
    DEFINE_MESSAGE(WM_COPYDATA),
    DEFINE_MESSAGE(WM_COPY),
    DEFINE_MESSAGE(WM_PASTE),
    DEFINE_MESSAGE(WM_CLEAR),
    DEFINE_MESSAGE(WM_UNDO),
    DEFINE_MESSAGE(WM_RENDERFORMAT),
    DEFINE_MESSAGE(WM_RENDERALLFORMATS),
    DEFINE_MESSAGE(WM_DESTROYCLIPBOARD),
    DEFINE_MESSAGE(WM_DRAWCLIPBOARD),
    DEFINE_MESSAGE(WM_PAINTCLIPBOARD),
    DEFINE_MESSAGE(WM_VSCROLLCLIPBOARD),
    DEFINE_MESSAGE(WM_SIZECLIPBOARD),
    DEFINE_MESSAGE(WM_ASKCBFORMATNAME),
    DEFINE_MESSAGE(WM_CHANGECBCHAIN),
    DEFINE_MESSAGE(WM_HSCROLLCLIPBOARD),
    DEFINE_MESSAGE(WM_QUERYNEWPALETTE),
    DEFINE_MESSAGE(WM_PALETTEISCHANGING),
    DEFINE_MESSAGE(WM_PALETTECHANGED),
    DEFINE_MESSAGE(WM_DDE_INITIATE),
    DEFINE_MESSAGE(WM_DDE_TERMINATE),
    DEFINE_MESSAGE(WM_DDE_ADVISE),
    DEFINE_MESSAGE(WM_DDE_UNADVISE),
    DEFINE_MESSAGE(WM_DDE_ACK),
    DEFINE_MESSAGE(WM_DDE_DATA),
    DEFINE_MESSAGE(WM_DDE_REQUEST),
    DEFINE_MESSAGE(WM_DDE_POKE),
    DEFINE_MESSAGE(WM_DDE_EXECUTE),
    DEFINE_MESSAGE(WM_DROPFILES),
    DEFINE_MESSAGE(WM_POWER),
    DEFINE_MESSAGE(WM_WINDOWPOSCHANGED),
    DEFINE_MESSAGE(WM_WINDOWPOSCHANGING),
    // MFC specific messages
    /*DEFINE_MESSAGE(WM_SIZEPARENT),
    DEFINE_MESSAGE(WM_SETMESSAGESTRING),
    DEFINE_MESSAGE(WM_IDLEUPDATECMDUI),
    DEFINE_MESSAGE(WM_INITIALUPDATE),
    DEFINE_MESSAGE(WM_COMMANDHELP),
    DEFINE_MESSAGE(WM_HELPHITTEST),
    DEFINE_MESSAGE(WM_EXITHELPMODE),*/
    DEFINE_MESSAGE(WM_HELP),
    DEFINE_MESSAGE(WM_NOTIFY),
    DEFINE_MESSAGE(WM_CONTEXTMENU),
    DEFINE_MESSAGE(WM_TCARD),
    DEFINE_MESSAGE(WM_MDIREFRESHMENU),
    DEFINE_MESSAGE(WM_MOVING),
    DEFINE_MESSAGE(WM_STYLECHANGED),
    DEFINE_MESSAGE(WM_STYLECHANGING),
    DEFINE_MESSAGE(WM_SIZING),
    DEFINE_MESSAGE(WM_SETHOTKEY),
    DEFINE_MESSAGE(WM_PRINT),
    DEFINE_MESSAGE(WM_PRINTCLIENT),
    DEFINE_MESSAGE(WM_POWERBROADCAST),
    DEFINE_MESSAGE(WM_HOTKEY),
    DEFINE_MESSAGE(WM_GETICON),
    DEFINE_MESSAGE(WM_EXITMENULOOP),
    DEFINE_MESSAGE(WM_ENTERMENULOOP),
    DEFINE_MESSAGE(WM_DISPLAYCHANGE),
    DEFINE_MESSAGE(WM_STYLECHANGED),
    DEFINE_MESSAGE(WM_STYLECHANGING),
    DEFINE_MESSAGE(WM_GETICON),
    DEFINE_MESSAGE(WM_SETICON),
    DEFINE_MESSAGE(WM_SIZING),
    DEFINE_MESSAGE(WM_MOVING),
    DEFINE_MESSAGE(WM_CAPTURECHANGED),
    DEFINE_MESSAGE(WM_DEVICECHANGE),
    DEFINE_MESSAGE(WM_PRINT),
    DEFINE_MESSAGE(WM_PRINTCLIENT),
    {
        0,
        NULL,
    }  // end of message list
};
std::string CWindowsMessageToString::GetStringFromMsg(
    DWORD dwMessage, bool bShowFrequentMessages) {
  if (!bShowFrequentMessages &&
      (dwMessage == WM_MOUSEMOVE || dwMessage == WM_NCMOUSEMOVE ||
       dwMessage == WM_NCHITTEST || dwMessage == WM_SETCURSOR ||
       dwMessage == WM_CTLCOLORBTN || dwMessage == WM_CTLCOLORDLG ||
       dwMessage == WM_CTLCOLOREDIT || dwMessage == WM_CTLCOLORLISTBOX ||
       dwMessage == WM_CTLCOLORMSGBOX || dwMessage == WM_CTLCOLORSCROLLBAR ||
       dwMessage == WM_CTLCOLORSTATIC || dwMessage == WM_ENTERIDLE ||
       dwMessage == WM_CANCELMODE ||
       dwMessage == 0x0118))  // WM_SYSTIMER (caret blink)
  {
    // don't report very frequently sent messages
    return "";
  }
  const AFX_MAP_MESSAGE* pMapMsg = allMessages;
  for (/*null*/; pMapMsg->lpszMsg != NULL; pMapMsg++) {
    if (pMapMsg->nMsg == dwMessage) {
      return pMapMsg->lpszMsg;
    }
  }

  return std::to_string(dwMessage);
}

#else
std::string GetLastErrorAsString() { return ""; }
#endif

//-----------------------------------------------------------------------------
std::string OrbitUtils::GetTimeStamp() {
  time_t rawtime;
  time(&rawtime);
  return FormatTime(rawtime);
}

//-----------------------------------------------------------------------------
std::string OrbitUtils::FormatTime(const time_t& rawtime) {
  struct tm* time_info = nullptr;
  char buffer[80];
#ifdef _WIN32
  struct tm timeinfo;
  localtime_s(&timeinfo, &rawtime);
  time_info = &timeinfo;
#else
  time_info = localtime(&rawtime);
#endif

  strftime(buffer, 80, "%Y_%m_%d_%H_%M_%S", time_info);

  return std::string(buffer);
}

//-----------------------------------------------------------------------------
bool ReadProcessMemory(uint32_t pid, uint64_t address, byte* buffer,
                       uint64_t size, size_t* num_bytes_read) {
#if _WIN32
  HANDLE h_process = reinterpret_cast<HANDLE>(pid);
  BOOL res = ReadProcessMemory(h_process, reinterpret_cast<void*>(address),
                               buffer, size, num_bytes_read);
  return res == TRUE;
#else
  iovec local_iov[] = {{buffer, size}};
  iovec remote_iov[] = {{reinterpret_cast<void*>(address), size}};
  *num_bytes_read = process_vm_readv(pid, local_iov, ABSL_ARRAYSIZE(local_iov),
                                     remote_iov, ABSL_ARRAYSIZE(remote_iov), 0);
  return *num_bytes_read == size;
#endif
}
