#ifndef _listen_h_
#define _listen_h_

static const WCHAR xyzzysrv_name[] = L"xyzzy-server v3";

// wm_private_xyzzysrv
//  wparam: process ID
//  lparam: handle

struct xyzzysrv_param
{
  int size;
  DWORD pid;
  HANDLE hevent;
  HWND hwnd;
  int kill_ok;
  WCHAR data[1];
};

#endif
