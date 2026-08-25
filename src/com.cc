#include "stdafx.h"
#include "ed.h"
#include "com.h"
#include "oleconv.h"
#include "sysdep.h"

static void
set_desc (IShellLinkW *sl, lisp ldesc)
{
  WCHAR *b = (WCHAR *)alloca (sizeof (WCHAR) * (w2ul (ldesc) + 1));
  w2u (b, ldesc);
  ole_error (sl->SetDescription (b));
}

static void
set_args (IShellLinkW *sl, lisp largs)
{
  WCHAR *b = (WCHAR *)alloca (sizeof (WCHAR) * (w2ul (largs) + 1));
  w2u (b, largs);
  ole_error (sl->SetArguments (b));
}

static void
set_appid (IShellLinkW *sl, lisp lappid)
{
  if (!sysdep.Win6_1p ())
    return;

  WCHAR *w = (WCHAR *)alloca (sizeof (WCHAR) * (w2ul (lappid) + 1));
  w2u (w, lappid);

  safe_com <IPropertyStore> store;
  ole_error (sl->QueryInterface (IID_PPV_ARGS(&store)));

  PROPVARIANT pv;
  PropVariantClear (&pv);

  pv.vt = VT_LPWSTR;
  pv.pwszVal = w;

  ole_error (store->SetValue (PKEY_AppUserModel_ID, pv));
  ole_error (store->Commit ());
}

lisp
Fcreate_shortcut (lisp lobject, lisp llink, lisp keys)
{
  check_string (lobject);
  check_string (llink);
  lisp ldesc = find_keyword (Kdescription, keys, 0);
  if (ldesc)
    check_string (ldesc);
  lisp largs = find_keyword (Karguments, keys, 0);
  if (largs)
    check_string (largs);
  lisp lworkdir = find_keyword (Kworking_directory, keys, 0);
  if (lworkdir)
    check_string (lworkdir);
  int show;
  lisp lshow = find_keyword (Kshow, keys, 0);
  if (lshow == Kshow)
    show = SW_SHOWNORMAL;
  else if (lshow == Kmaximize)
    show = SW_SHOWMAXIMIZED;
  else if (lshow == Kminimize)
    show = SW_SHOWMINIMIZED;
  else
    show = SW_SHOWNORMAL;
  lisp lappid = find_keyword (Kappid, keys, 0);
  if (lappid)
    check_string (lappid);

  safe_com <IShellLinkW> sl;
  ole_error (CoCreateInstance (CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER,
                               IID_IShellLinkW, (void **)&sl));
  char path[PATH_MAX + 1];
  WCHAR wpath[PATH_MAX + 1];
  pathname2cstr (lobject, path);

  if (Ffile_directory_p (lobject))
    {
      safe_com <IMalloc> ialloc;
      ole_error (SHGetMalloc (&ialloc));

      safe_com <IShellFolder> sf;
      ole_error (SHGetDesktopFolder (&sf));

      map_sl_to_backsl (path);
      u82u (wpath, path);

      ULONG ul;
      safe_idl idl (ialloc);
      ole_error (sf->ParseDisplayName (get_active_window (), 0, wpath, &ul, &idl, 0));
      ole_error (sl->SetIDList (idl));
    }
  else
    {
      u82u (wpath, path);
      ole_error (sl->SetPath (wpath));
    }

  if (ldesc)
    set_desc (sl, ldesc);
  if (largs)
    set_args (sl, largs);
  if (lworkdir)
    {
      pathname2cstr (lworkdir, path);
      u82u (wpath, path);
      ole_error (sl->SetWorkingDirectory (wpath));
    }
  if (lshow)
    ole_error (sl->SetShowCmd (show));
  if (lappid)
    set_appid (sl, lappid);

  safe_com <IPersistFile> pf;
  ole_error (sl->QueryInterface (IID_IPersistFile, (void **)&pf));

  pathname2cstr (llink, path);
  u82u (wpath, path);
  ole_error (pf->Save (wpath, 1));

  return Qt;
}

lisp
Fget_special_folder_location (lisp place)
{
  int f = 0;
  if (place == Kdesktop)
    f = CSIDL_DESKTOPDIRECTORY;
  else if (place == Knetwork)
    f = CSIDL_NETHOOD;
  else if (place == Kpersonal)
    f = CSIDL_PERSONAL;
  else if (place == Kprograms)
    f = CSIDL_PROGRAMS;
  else if (place == Krecent)
    f = CSIDL_RECENT;
  else if (place == Ksend_to)
    f = CSIDL_SENDTO;
  else if (place == Kstart_menu)
    f = CSIDL_STARTMENU;
  else if (place == Kstartup)
    f = CSIDL_STARTUP;
  else if (place == Ktemplates)
    f = CSIDL_TEMPLATES;
  else
    FEprogram_error (Eunknown_folder_name, place);

  safe_com <IMalloc> ialloc;
  ole_error (SHGetMalloc (&ialloc));

  safe_idl idl (ialloc);
  ole_error (SHGetSpecialFolderLocation (get_active_window (), f, &idl));

  safe_com <IShellFolder> sf;
  ole_error (SHGetDesktopFolder (&sf));

  STRRET name;
  ole_error (sf->GetDisplayNameOf (idl, SHGDN_FORPARSING, &name), place);

  switch (name.uType)
    {
    default:
      FEprogram_error (Eunknown_STRRET_type, make_fixnum (name.uType));

    case STRRET_WSTR:
      {
        lisp r = make_string (name.pOleStr);
        ialloc->Free (name.pOleStr);
        return r;
      }

    case STRRET_OFFSET:
      return make_string ((char *)(ITEMIDLIST *)idl + name.uOffset);

    case STRRET_CSTR:
      return make_string (name.cStr);
    }
}

lisp
Fresolve_shortcut (lisp lshortcut)
{
  char shortcut[PATH_MAX + 1];
  pathname2cstr (lshortcut, shortcut);
  map_sl_to_backsl (shortcut);
  WCHAR w[PATH_MAX + 1];
  u82u (w, shortcut);

  safe_com <IShellLinkW> sl;
  ole_error (CoCreateInstance (CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER,
                               IID_IShellLinkW, (void **)&sl));

  safe_com <IPersistFile> pf;
  ole_error (sl->QueryInterface (IID_IPersistFile, (void **)&pf));
  HRESULT hr = pf->Load (w, STGM_READ);
  if (hr == E_FAIL)
    FEfile_error (Enot_a_shortcut, lshortcut);
  ole_error (hr);

  WIN32_FIND_DATAW fd;
  WCHAR path[PATH_MAX], desc[PATH_MAX];
  ole_error (sl->GetPath (path, numberof (path), &fd, 0));
  if (!*path)
    FEfile_error (Enot_a_shortcut, lshortcut);
  ole_error (sl->GetDescription (desc, numberof (desc)));

  map_backsl_to_sl (path);
  multiple_value::count () = 2;
  multiple_value::value (1) = make_string (desc);
  return make_string (path);
}

lisp
Fole_drop_files (lisp lpath, lisp lclsid, lisp ldir, lisp lfiles)
{
  USES_CONVERSION;

  char path[MAX_PATH + 1];
  pathname2cstr (lpath, path);
  map_sl_to_backsl (path);
  wchar_t *wpath = A2W (path);

  check_string (lclsid);
  wchar_t *wclsid = I2W (lclsid);
  CLSID clsid;
  if (FAILED (CLSIDFromString (wclsid, &clsid)))
    ole_error (CLSIDFromProgID (wclsid, &clsid));

  char dir[PATH_MAX + 1];
  pathname2cstr (ldir, dir);
  map_sl_to_backsl (dir);
  int maxl = strlen (dir);

  lisp f = lfiles;
  int nfiles;
  for (nfiles = 0; consp (f); f = xcdr (f), nfiles++)
    {
      check_string (xcar (f));
      maxl = max (maxl, int (w2ul (xcar (f))));
    }

  if (!nfiles)
    return Qnil;

  safe_com <IDropTarget> dt;
  ole_error (CoCreateInstance (clsid, 0, CLSCTX_INPROC_SERVER,
                               IID_IDropTarget, (void **)&dt));

  safe_com <IShellFolder> desktop;
  ole_error (SHGetDesktopFolder (&desktop));

  safe_com <IMalloc> ialloc;
  ole_error (SHGetMalloc (&ialloc));

  maxl++;
  wchar_t *wbuf = (wchar_t *)alloca (sizeof *wbuf * maxl);
  u82u (wbuf, dir);

  ULONG eaten;
  safe_idl dir_idl (ialloc);
  ole_error (desktop->ParseDisplayName (0, 0, wbuf, &eaten, &dir_idl, 0));

  safe_com <IShellFolder> sf;
  ole_error (desktop->BindToObject (dir_idl, 0, IID_IShellFolder, (void **)&sf));

  ITEMIDLIST **idls = (ITEMIDLIST **)alloca (sizeof *idls * nfiles);
  safe_vidl (ialloc, idls, nfiles);

  f = lfiles;
  int i;
  for (i = 0; i < nfiles && consp (f); i++, f = xcdr (f))
    {
      w2u (wbuf, xcar (f));
      ole_error (sf->ParseDisplayName (0, 0, wbuf, &eaten, &idls[i], 0));
    }

  safe_com <IDataObject> data_obj;
  ole_error (sf->GetUIObjectOf (0, i, (const ITEMIDLIST **)idls,
                                IID_IDataObject, 0, (void **)&data_obj));

  safe_com <IPersistFile> pf;
  ole_error (dt->QueryInterface (IID_IPersistFile, (void **)&pf));
  ole_error (pf->Load (wpath, STGM_READ));

  POINTL pt = {0};
  DWORD effect = DROPEFFECT_COPY;
  ole_error (dt->DragEnter (data_obj, 0, pt, &effect));
  if (effect & DROPEFFECT_COPY)
    {
      effect = DROPEFFECT_COPY;
      ole_error (dt->Drop (data_obj, 0, pt, &effect));
      return Qt;
    }
  else
    {
      ole_error (dt->DragLeave ());
      return Qnil;
    }
}
