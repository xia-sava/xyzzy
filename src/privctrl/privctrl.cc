#include "stdafx.h"
#define EXTERN
#include "privctlimpl.h"

static BOOL PrivateControlsInitialized;
static const WCHAR PropName[] = L"PropPrivCtrl";
static const WCHAR OwnerDrawName[] = L"PropOwnerDraw";

static LRESULT CALLBACK
ParentWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg)
    {
    case WM_NCDESTROY:
      return CallWindowProcW (WNDPROC (RemovePropW (hwnd, ATOM2STR (hprop))),
                              hwnd, msg, wparam, lparam);

    case WM_DRAWITEM:
      {
        OWNERDRAWPROC proc = get_owner_draw_proc (((DRAWITEMSTRUCT *)lparam)->hwndItem);
        if (proc)
          return proc (wparam, (DRAWITEMSTRUCT *)lparam);
        break;
      }
    }
  return CallWindowProcW (WNDPROC (GetPropW (hwnd, ATOM2STR (hprop))),
                          hwnd, msg, wparam, lparam);
}

int
subclass_parent (HWND hwnd)
{
  HWND parent = GetParent (hwnd);
  if (!parent)
    return 0;
  if (GetPropW (parent, ATOM2STR (hprop)))
    return 1;
  if (!SetPropW (parent, ATOM2STR (hprop),
                 HANDLE (GetWindowLongW (parent, GWL_WNDPROC))))
    return 0;
  return SetWindowLongW (parent, GWL_WNDPROC, LONG (ParentWndProc));
}

item_data *
alloc_item_data (HWND hwnd, DWORD size)
{
  item_data *p = (item_data *)LocalAlloc (LMEM_FIXED | LMEM_ZEROINIT, size);
  if (!p)
    return 0;
  if (!set_item_data (hwnd, p))
    {
      LocalFree (HLOCAL (p));
      return 0;
    }
  return p;
}

#ifdef PRIVCTRL_NODLL
# define InitPrivateControls __InitPrivateControls
#endif

void PRIVCTRLAPI
InitPrivateControls ()
{
  if (PrivateControlsInitialized)
    return;

  InitCommonControls ();

  if (!init_listview_class ()
      || !init_url_class ())
    return;

  PrivateControlsInitialized = 1;
}

BOOL
DllMain (HINSTANCE hinst, DWORD reason, LPVOID)
{
  switch (reason)
    {
    case DLL_PROCESS_ATTACH:
      hinstDLL = hinst;
      hprop = GlobalAddAtomW (PropName);
      if (!hprop)
        return 0;
      hownerdraw = GlobalAddAtomW (OwnerDrawName);
      if (!hownerdraw)
        return 0;
      InitPrivateControls ();
      break;

    case DLL_PROCESS_DETACH:
      GlobalDeleteAtom (hprop);
      GlobalDeleteAtom (hownerdraw);
      cleanup_url_class ();
      break;
    }
  return 1;
}

#ifdef PRIVCTRL_NODLL
# undef InitPrivateControls

static void __cdecl
cleanup ()
{
  DllMain (0, DLL_PROCESS_DETACH, 0);
}

void PRIVCTRLAPI
InitPrivateControls (HINSTANCE hinst)
{
  static int done;
  if (!done)
    {
      done = 1;
      DllMain (hinst, DLL_PROCESS_ATTACH, 0);
      atexit (cleanup);
    }
}

#endif

