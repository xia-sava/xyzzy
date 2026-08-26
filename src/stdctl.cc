#include "stdafx.h"
#include "ed.h"

static const WCHAR csComboBox[] = L"ComboBox";
static const WCHAR csEdit[] = L"Edit";
static const WCHAR csListBox[] = L"ListBox";
static const WCHAR PropOrgWndProc[] = L"PropStdctlWndProc";

static HHOOK stdctl_cbt_hook;

static inline LRESULT
org_sendmsg (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  return CallWindowProcW (WNDPROC (GetPropW (hwnd, PropOrgWndProc)),
                          hwnd, msg, wparam, lparam);
}

static inline LRESULT
org_ncdestroy (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  return CallWindowProcW (WNDPROC (RemovePropW (hwnd, PropOrgWndProc)),
                          hwnd, msg, wparam, lparam);
}

static int
stdctl_char_eq (int ch, lisp x)
{
  x = xsymbol_value (x);
  return charp (x) && ch == xchar_code (x);
}

int
stdctl_operation (int ch)
{
  if (stdctl_char_eq (ch, Vstd_control_up_char))
    return VK_UP;
  if (stdctl_char_eq (ch, Vstd_control_down_char))
    return VK_DOWN;
  if (stdctl_char_eq (ch, Vstd_control_default_char))
    return VK_RETURN;
  if (stdctl_char_eq (ch, Vstd_control_prior_char))
    return VK_PRIOR;
  if (stdctl_char_eq (ch, Vstd_control_next_char))
    return VK_NEXT;
  return -1;
}

static void
stdctl_default (HWND hwnd)
{
  HWND hwnd_parent = GetParent (hwnd);
  if (hwnd_parent)
    {
      DWORD id = SendMessage (hwnd_parent, DM_GETDEFID, 0, 0);
      if (HIWORD (id) == DC_HASDEFID)
        {
          HWND hwnd_btn = GetDlgItem (hwnd_parent, LOWORD (id));
          if (hwnd_btn)
            PostMessage (hwnd_parent, WM_COMMAND,
                         MAKEWPARAM (LOWORD (id), BN_CLICKED),
                         LPARAM (hwnd_btn));
        }
    }
}

static int
lbx_keydown (HWND hwnd, int vk)
{
  if (!(GetWindowLong (hwnd, GWL_STYLE) & LBS_EXTENDEDSEL)
      || (vk != VK_UP && vk != VK_DOWN && vk != VK_SPACE)
      || GetKeyState (VK_CONTROL) >= 0
      || GetKeyState (VK_SHIFT) < 0)
    return 0;

  int i = org_sendmsg (hwnd, LB_GETCARETINDEX, 0, 0);
  switch (vk)
    {
    case VK_UP:
      if (i > 0)
        org_sendmsg (hwnd, LB_SETCARETINDEX, i - 1, 0);
      break;

    case VK_DOWN:
      if (i < org_sendmsg (hwnd, LB_GETCOUNT, 0, 0) - 1)
        org_sendmsg (hwnd, LB_SETCARETINDEX, i + 1, 0);
      break;

    default:
      org_sendmsg (hwnd, LB_SETSEL, !org_sendmsg (hwnd, LB_GETSEL, i, 0), i);
      break;
    }
  return 1;
}

static int
lbx_char (HWND hwnd, int ch)
{
  int op = stdctl_operation (ch);
  if (op < 0)
    {
      HWND hwnd_parent = GetParent (hwnd);
      if (!hwnd_parent)
        return 0;
      PostMessage (hwnd_parent, WM_PRIVATE_LISTBOX_CHAR,
                   GetWindowLong (hwnd, GWL_ID), ch);
      return 1;
    }

  if (op == VK_RETURN)
    stdctl_default (hwnd);
  else
    {
      org_sendmsg (hwnd, WM_KEYDOWN, op, 0);
      org_sendmsg (hwnd, WM_KEYUP, op, 0);
    }
  return 1;
}

static LRESULT CALLBACK
lbx_wndproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg)
    {
    case WM_NCDESTROY:
      return org_ncdestroy (hwnd, msg, wparam, lparam);

    case WM_KEYDOWN:
      if (lbx_keydown (hwnd, wparam))
        return 0;
      break;

    case WM_CHAR:
      if (lbx_char (hwnd, wparam))
        return 0;
      break;
    }
  return org_sendmsg (hwnd, msg, wparam, lparam);
}

static int
cbx_char (HWND hwnd, int ch)
{
  int op = stdctl_operation (ch);
  if (op < 0)
    return 0;
  if (op == VK_RETURN)
    stdctl_default (hwnd);
  else
    {
      org_sendmsg (hwnd, WM_KEYDOWN, op, 0);
      org_sendmsg (hwnd, WM_KEYUP, op, 0);
    }
  return 1;
}

static LRESULT CALLBACK
cbx_wndproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  if (msg == WM_NCDESTROY)
    return org_ncdestroy (hwnd, msg, wparam, lparam);
  if (msg == WM_CHAR && cbx_char (hwnd, wparam))
    return 0;
  return org_sendmsg (hwnd, msg, wparam, lparam);
}

static int
edt_char (HWND hwnd, int ch)
{
  int op = stdctl_operation (ch);
  if (op < 0)
    return 0;

  WCHAR class_name[16];
  HWND hwnd_parent = GetParent (hwnd);
  if (!hwnd_parent
      || !GetClassNameW (hwnd_parent, class_name, numberof (class_name))
      || _wcsicmp (class_name, csComboBox))
    return 0;

  if (op == VK_RETURN)
    stdctl_default (hwnd_parent);
  else
    {
      org_sendmsg (hwnd, WM_KEYDOWN, op, 0);
      org_sendmsg (hwnd, WM_KEYUP, op, 0);
    }
  return 1;
}

static LRESULT CALLBACK
edt_wndproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  if (msg == WM_NCDESTROY)
    return org_ncdestroy (hwnd, msg, wparam, lparam);
  if (msg == WM_CHAR && edt_char (hwnd, wparam))
    return 0;
  return org_sendmsg (hwnd, msg, wparam, lparam);
}

static void
subclass (HWND hwnd)
{
  WCHAR class_name[16];
  if (!GetClassNameW (hwnd, class_name, numberof (class_name)))
    return;

  WNDPROC wndproc;
  if (!_wcsicmp (class_name, csComboBox))
    wndproc = cbx_wndproc;
  else if (!_wcsicmp (class_name, csEdit))
    wndproc = edt_wndproc;
  else if (!_wcsicmp (class_name, csListBox))
    wndproc = lbx_wndproc;
  else
    return;

  if (!SetPropW (hwnd, PropOrgWndProc,
                 HANDLE (GetWindowLongW (hwnd, GWL_WNDPROC))))
    return;
  SetWindowLongW (hwnd, GWL_WNDPROC, LONG (wndproc));
}

// コモンコントロールの版 6 はクラス名を版付きの名前へ引き直すので、標準の
// クラスを同名で登録し直しても、ダイアログが作る窓はそちらへ届かない。
// 作られた窓を一つずつ差し替える。窓の作り手で絞り、xyzzy が作るダイアログ
// だけを相手にする。
static LRESULT CALLBACK
cbt_proc (int code, WPARAM wparam, LPARAM lparam)
{
  if (code == HCBT_CREATEWND
      && ((CBT_CREATEWND *)lparam)->lpcs->hInstance == app.hinst)
    subclass (HWND (wparam));
  return CallNextHookEx (stdctl_cbt_hook, code, wparam, lparam);
}

// ダイアログを作るスレッドで呼ぶ。フックはそのスレッドにだけ掛かる。
void
stdctl_hook_init ()
{
  stdctl_cbt_hook = SetWindowsHookExW (WH_CBT, cbt_proc, 0,
                                       GetCurrentThreadId ());
}
