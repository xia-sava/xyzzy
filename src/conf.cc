#include "stdafx.h"
#include "ed.h"
#include "environ.h"
#include "print.h"
#include "monitor.h"
#include "inifile.h"

#define DECLARE_CONF(NAME, VALUE) WCHAR NAME[] = L"" VALUE;
#include "conf.h"

static IniFile ini_file;

static IniFile &
ini ()
{
  if (!ini_file.loaded () && app.ini_file_path)
    {
      WCHAR path[PATH_MAX];
      s2u (path, app.ini_file_path);
      ini_file.open (path);
    }
  return ini_file;
}

/* フォントの名前は LOGFONT が CP932 で持つ。設定へ出し入れするときだけ移す */
static void
face2u (WCHAR *b, const char *face)
{
  if (!MultiByteToWideChar (CP_ACP, 0, face, -1, b, LF_FACESIZE))
    *b = 0;
}

static void
u2face (char *face, const WCHAR *b)
{
  if (!WideCharToMultiByte (CP_ACP, 0, b, -1, face, LF_FACESIZE, 0, 0))
    *face = 0;
}

void
write_conf (const WCHAR *section, const WCHAR *name, const WCHAR *str)
{
  ini ().set (section, name, str);
}

void
write_conf (const WCHAR *section, const WCHAR *name, long value, int hex)
{
  WCHAR buf[32];
  wsprintfW (buf, hex ? L"#%lx" : L"%ld", value);
  ini ().set (section, name, buf);
}

void
write_conf (const WCHAR *section, const WCHAR *name, const int *value, int n, int hex)
{
  WCHAR *buf = (WCHAR *)alloca (16 * n * sizeof (WCHAR)), *b = buf;
  for (int i = 0; i < n; i++)
    b += wsprintfW (b, hex ? L",#%x" : L",%d", *value++);
  ini ().set (section, name, buf + 1);
}

void
write_conf (const WCHAR *section, const WCHAR *name, const RECT &r)
{
  WCHAR buf[128];
  wsprintfW (buf, L"(%d,%d)-(%d,%d)", r.left, r.top, r.right, r.bottom);
  ini ().set (section, name, buf);
}

void
write_conf (const WCHAR *section, const WCHAR *name, const LOGFONT &lf)
{
  WCHAR face[LF_FACESIZE];
  face2u (face, lf.lfFaceName);
  WCHAR buf[128];
  wsprintfW (buf, L"%d,\"%s\",%d", lf.lfHeight, face, lf.lfCharSet);
  ini ().set (section, name, buf);
}

void
write_conf (const WCHAR *section, const WCHAR *name, const PRLOGFONT &lf)
{
  WCHAR face[LF_FACESIZE];
  face2u (face, lf.face);
  WCHAR buf[128];
  wsprintfW (buf, L"%d,\"%s\",%d,%d,%d", lf.point, face, lf.charset, lf.bold, lf.italic);
  ini ().set (section, name, buf);
}

void
write_conf (const WCHAR *section, const WCHAR *name, const WINDOWPLACEMENT &w)
{
  WCHAR buf[128];
  wsprintfW (buf, L"(%d,%d)-(%d,%d),%d",
             w.rcNormalPosition.left,
             w.rcNormalPosition.top,
             w.rcNormalPosition.right,
             w.rcNormalPosition.bottom,
             w.showCmd);
  ini ().set (section, name, buf);
}

void
flush_conf ()
{
  ini ().flush ();
}

int
read_conf (const WCHAR *section, const WCHAR *name, WCHAR *buf, int size)
{
  return ini ().get (section, name, buf, size);
}

void
delete_conf (const WCHAR *section)
{
  ini ().erase (section);
}

static int
parse_int (const WCHAR *s, int &v)
{
  return swscanf (s, *s == '#' ? L"#%x" : L"%d", &v) == 1;
}

int
read_conf (const WCHAR *section, const WCHAR *name, int &value)
{
  WCHAR buf[32];
  int l = read_conf (section, name, buf, numberof (buf));
  if (!l || l >= int (numberof (buf)) - 1)
    return 0;
  return parse_int (buf, value);
}

#if INT_MAX != LONG_MAX
static int
parse_long (const WCHAR *s, u_long &v)
{
  return swscanf (s, *s == '#' ? L"#%lx" : L"%ld", &v) == 1;
}

int
read_conf (const WCHAR *section, const WCHAR *name, u_long &value)
{
  WCHAR buf[32];
  int l = read_conf (section, name, buf, numberof (buf));
  if (!l || l >= int (numberof (buf)) - 1)
    return 0;
  return parse_long (buf, value);
}
#endif /* INT_MAX != LONG_MAX */

int
read_conf (const WCHAR *section, const WCHAR *name, int *value, int n)
{
  int size = 16 * n;
  WCHAR *buf = (WCHAR *)alloca (size * sizeof (WCHAR));
  int l = read_conf (section, name, buf, size);
  if (!l || l >= size - 1)
    return 0;
  for (int i = 1; i < n; i++, buf++, value++)
    {
      if (!parse_int (buf, *value))
        return 0;
      buf = wcschr (buf, ',');
      if (!buf)
        return 0;
    }
  return parse_int (buf, *value);
}

int
read_conf (const WCHAR *section, const WCHAR *name, RECT &rr)
{
  WCHAR buf[128];
  int l = read_conf (section, name, buf, numberof (buf));
  if (!l || l >= int (numberof (buf)) - 1)
    return 0;
  int t, r, b;
  if (swscanf (buf, L"(%d,%d)-(%d,%d)", &l, &t, &r, &b) != 4)
    return 0;
  rr.left = l;
  rr.top = t;
  rr.right = r;
  rr.bottom = b;
  return 1;
}

int
read_conf (const WCHAR *section, const WCHAR *name, LOGFONT &lf)
{
  WCHAR buf[128];
  int l = read_conf (section, name, buf, numberof (buf));
  if (!l || l >= int (numberof (buf)) - 1)
    return 0;
  memset (&lf, 0, sizeof lf);
  WCHAR face[LF_FACESIZE];
  int h, cs;
  if (swscanf (buf, L"%d,\"%31[^\"]\",%d", &h, face, &cs) != 3)
    return 0;
  u2face (lf.lfFaceName, face);
  lf.lfHeight = h;
  lf.lfCharSet = cs;
  return 1;
}

int
read_conf (const WCHAR *section, const WCHAR *name, PRLOGFONT &lf)
{
  WCHAR buf[128];
  int l = read_conf (section, name, buf, numberof (buf));
  if (!l || l >= int (numberof (buf)) - 1)
    return 0;
  WCHAR face[LF_FACESIZE];
  int point, cs, bold, italic;
  if (swscanf (buf, L"%d,\"%31[^\"]\",%d,%d,%d",
               &point, face, &cs, &bold, &italic) != 5)
    return 0;
  u2face (lf.face, face);
  lf.point = point;
  lf.charset = cs;
  lf.bold = bold;
  lf.italic = italic;
  return 1;
}

int
read_conf (const WCHAR *section, const WCHAR *name, WINDOWPLACEMENT &w)
{
  WCHAR buf[128];
  int l = read_conf (section, name, buf, numberof (buf));
  if (!l || l >= int (numberof (buf)) - 1)
    return 0;
  int t, r, b, s;
  if (swscanf (buf, L"(%d,%d)-(%d,%d),%d", &l, &t, &r, &b, &s) != 5)
    return 0;
  w.rcNormalPosition.left = l;
  w.rcNormalPosition.top = t;
  w.rcNormalPosition.right = r;
  w.rcNormalPosition.bottom = b;
  w.showCmd = s;
  return 1;
}

void
conf_write_string (const WCHAR *section, const WCHAR *name, const WCHAR *string)
{
  int l = wcslen (string);
  WCHAR *b = (WCHAR *)alloca ((l + 3) * sizeof (WCHAR));
  *b = '"';
  memcpy (b + 1, string, l * sizeof (WCHAR));
  b[l + 1] = '"';
  b[l + 2] = 0;
  write_conf (section, name, b);
}

static void
adjust_geometry (RECT &r, const RECT &or, int posp, int sizep)
{
  if (!sizep)
    {
      r.right = r.left + or.right - or.left;
      r.bottom = r.top + or.bottom - or.top;
    }
  if (!posp)
    {
      r.right += or.left - r.left;
      r.bottom += or.top - r.top;
      r.left = or.left;
      r.top = or.top;
    }
}

int
conf_load_geometry (HWND hwnd, const WCHAR *section,
                    const WCHAR *prefix, int posp, int sizep)
{
  if (!posp && !sizep)
    return 0;

  WINDOWPLACEMENT w;
  w.length = sizeof w;
  if (!GetWindowPlacement (hwnd, &w))
    return 0;

  RECT cr (w.rcNormalPosition);

  WCHAR b[64];
  make_geometry_key (b, numberof (b), prefix);
  if (!read_conf (section, b, w))
    return 0;

  adjust_geometry (w.rcNormalPosition, cr, posp, sizep);

  w.flags = 0;
  if (w.showCmd == SW_SHOWMINIMIZED)
    w.showCmd = SW_SHOW;
  return SetWindowPlacement (hwnd, &w);
}

void
conf_save_geometry (HWND hwnd, const WCHAR *section,
                    const WCHAR *prefix, int posp, int sizep)
{
  if (!posp && !sizep)
    return;

  WINDOWPLACEMENT w;
  w.length = sizeof w;
  if (!GetWindowPlacement (hwnd, &w))
    return;
  if (xsymbol_value (Vfiler_save_window_snap_size) != Qnil)
    adjust_snap_window_size (hwnd, w);

  WCHAR b[64];
  make_geometry_key (b, numberof (b), prefix);

  if (!posp || !sizep)
    {
      WINDOWPLACEMENT ow;
      if (read_conf (section, b, ow))
        adjust_geometry (w.rcNormalPosition, ow.rcNormalPosition, posp, sizep);
    }

  write_conf (section, b, w);
}

void
adjust_snap_window_size (HWND hwnd, WINDOWPLACEMENT &w)
{
  if (w.showCmd != SW_SHOWNORMAL) return;

  RECT r;
  if (!GetWindowRect (hwnd, &r)) return;

  w.rcNormalPosition.left = r.left;
  w.rcNormalPosition.top = r.top;
  w.rcNormalPosition.right = r.right;
  w.rcNormalPosition.bottom = r.bottom;

  MONITORINFO info;
  if (monitor.get_monitorinfo_from_window (hwnd, &info))
    {
      int taskbar_width = info.rcWork.left - info.rcMonitor.left;
      int taskbar_height = info.rcWork.top - info.rcMonitor.top;
      w.rcNormalPosition.left -= taskbar_width;
      w.rcNormalPosition.top -= taskbar_height;
      w.rcNormalPosition.right -= taskbar_width;
      w.rcNormalPosition.bottom -= taskbar_height;
    }
}

void
make_geometry_key (WCHAR *buf, size_t bufsize, const WCHAR *prefix)
{
  _snwprintf_s (buf, bufsize, _TRUNCATE,
                L"%s%dx%d", prefix ? prefix : L"",
                GetSystemMetrics (SM_CXSCREEN),
                GetSystemMetrics (SM_CYSCREEN));
}

#define CONF_SZ           0x10000
#define CONF_INT          0x20000
#define CONF_HEX          0x30000
#define CONF_LOGFONT      0x40000
#define CONF_PRINT_FONT   0x50000

struct conf
{
  const WCHAR *name;
  DWORD reg_type;
  int type;
};

static const conf misc[] =
{
  {cfgSaveWindowSize, REG_DWORD, CONF_INT},
  {cfgSaveWindowSnapSize, REG_DWORD, CONF_INT},
  {cfgSaveWindowPosition, REG_DWORD, CONF_INT},
  {cfgWindowFlags, REG_DWORD, CONF_HEX},
  {cfgFnkeyLabels, REG_DWORD, CONF_INT},
  {cfgFoldMode, REG_DWORD, CONF_INT},
  {cfgFoldLineNumMode, REG_DWORD, CONF_INT},
  {cfgRestoreWindowSize, REG_DWORD, CONF_INT},
  {cfgRestoreWindowPosition, REG_DWORD, CONF_INT},
};

static const conf buffer_selector[] =
{
  {cfgColumn, REG_BINARY, CONF_INT | 4},
};

static const conf colors[] =
{
  {cfgTextColor, REG_DWORD, CONF_HEX},
  {cfgBackColor, REG_DWORD, CONF_HEX},
  {cfgCtlColor, REG_DWORD, CONF_HEX},
  {cfgKwdColor1, REG_DWORD, CONF_HEX},
  {cfgKwdColor2, REG_DWORD, CONF_HEX},
  {cfgKwdColor3, REG_DWORD, CONF_HEX},
  {cfgStringColor, REG_DWORD, CONF_HEX},
  {cfgCommentColor, REG_DWORD, CONF_HEX},
  {cfgTagColor, REG_DWORD, CONF_HEX},
  {cfgCursorColor, REG_DWORD, CONF_HEX},
  {cfgCaretColor, REG_DWORD, CONF_HEX},
  {cfgImeCaretColor, REG_DWORD, CONF_HEX},
  {cfgModeLineFg, REG_DWORD, CONF_HEX},
  {cfgModeLineBg, REG_DWORD, CONF_HEX},
};

static const conf filer[] =
{
  {cfgTextColor, REG_DWORD, CONF_HEX},
  {cfgBackColor, REG_DWORD, CONF_HEX},
  {cfgCursorColor, REG_DWORD, CONF_HEX},
  {cfgColumnLeft, REG_BINARY, CONF_INT | 4},
  {cfgColumnRight, REG_BINARY, CONF_INT | 4},
  {cfgSortRight, REG_DWORD, CONF_INT},
  {cfgSortLeft, REG_DWORD, CONF_INT},
  {cfgColumn, REG_BINARY, CONF_INT | 4},
  {cfgSort, REG_DWORD, CONF_INT},
};

static const conf font[] =
{
  {cfgJapanese, REG_BINARY, CONF_LOGFONT},
  {cfgGb2312, REG_BINARY, CONF_LOGFONT},
  {cfgKsc5601, REG_BINARY, CONF_LOGFONT},
  {cfgCyrillic, REG_BINARY, CONF_LOGFONT},
  {cfgBig5, REG_BINARY, CONF_LOGFONT},
  {cfgAscii, REG_BINARY, CONF_LOGFONT},
  {cfgGreek, REG_BINARY, CONF_LOGFONT},
  {cfgLineFeed, REG_DWORD, CONF_INT},
  {cfgBackslash, REG_DWORD, CONF_INT},
  {cfgLatin, REG_BINARY, CONF_LOGFONT},
  {cfgLineSpacing, REG_DWORD, CONF_INT},
  {cfgRecommendSize, REG_DWORD, CONF_INT},
};

static const conf print[] =
{
  {cfgMargin, REG_BINARY, CONF_INT | 4},
  {cfgHeaderMargin, REG_DWORD, CONF_INT},
  {cfgFooterMargin, REG_DWORD, CONF_INT},
  {cfgLineNumber, REG_DWORD, CONF_INT},
  {cfgHeader, REG_SZ, CONF_SZ},
  {cfgFooter, REG_SZ, CONF_SZ},
  {cfgHeaderOn, REG_DWORD, CONF_INT},
  {cfgFooterOn, REG_DWORD, CONF_INT},
  {cfgColumns, REG_DWORD, CONF_INT},
  {cfgColumnSep, REG_DWORD, CONF_INT},
  {cfgFoldColumns, REG_DWORD, CONF_INT},
  {cfgAscii, REG_BINARY, CONF_PRINT_FONT},
  {cfgJapanese, REG_BINARY, CONF_PRINT_FONT},
  {cfgLatin, REG_BINARY, CONF_PRINT_FONT},
  {cfgCyrillic, REG_BINARY, CONF_PRINT_FONT},
  {cfgGreek, REG_BINARY, CONF_PRINT_FONT},
  {cfgGb2312, REG_BINARY, CONF_PRINT_FONT},
  {cfgBig5, REG_BINARY, CONF_PRINT_FONT},
  {cfgKsc5601, REG_BINARY, CONF_PRINT_FONT},
};

static const conf preview[] =
{
  {cfgScale, REG_DWORD, CONF_INT},
};

static void
reg2ini_str (const WCHAR *key, ReadRegistry &r, const conf &cf)
{
  DWORD type;
  int l = r.query (cf.name, &type);
  if (l > 0 && type == REG_SZ)
    {
      int n = l / sizeof (WCHAR) + 1;
      WCHAR *v = (WCHAR *)alloca (n * sizeof (WCHAR));
      if (r.get (cf.name, v, n) == l)
        conf_write_string (key, cf.name, v);
    }
}

static void
reg2ini_int (const WCHAR *key, ReadRegistry &r, const conf &cf)
{
  int v;
  if (r.get (cf.name, &v))
    write_conf (key, cf.name, v, cf.type == CONF_HEX);
}

static void
reg2ini_int (const WCHAR *key, ReadRegistry &r, const conf &cf, int l)
{
  int sz = sizeof (int) * l;
  int *v = (int *)alloca (sz);
  if (r.get (cf.name, v, sz) == sz)
    write_conf (key, cf.name, v, l, (cf.type & ~0xffff) == CONF_HEX);
}

static void
reg2ini_logfont (const WCHAR *key, ReadRegistry &r, const conf &cf)
{
  LOGFONT lf;
  if (r.get (cf.name, &lf, sizeof lf) == sizeof lf)
    write_conf (key, cf.name, lf);
}

static void
reg2ini_print_font (const WCHAR *key, ReadRegistry &r, const conf &cf)
{
  PRLOGFONT lf;
  if (r.get (cf.name, &lf, sizeof lf) == sizeof lf)
    write_conf (key, cf.name, lf);
}

static void
reg2ini (const WCHAR *rkey, const WCHAR *ikey, const conf *cf, int n)
{
  WCHAR *key;
  if (!*rkey)
    key = (WCHAR *)Registry::Settings;
  else
    {
      key = (WCHAR *)alloca ((wcslen (Registry::Settings) + wcslen (rkey) + 2)
                             * sizeof (WCHAR));
      wsprintfW (key, L"%s\\%s", Registry::Settings, rkey);
    }

  if (!ikey)
    ikey = rkey;

  ReadRegistry r (key);
  if (r.fail ())
    return;

  for (int i = 0; i < n; i++)
    switch (cf[i].type & ~0xffff)
      {
      case CONF_SZ:
        reg2ini_str (ikey, r, cf[i]);
        break;

      case CONF_INT:
      case CONF_HEX:
        if (!(cf[i].type & 0xffff))
          reg2ini_int (ikey, r, cf[i]);
        else
          reg2ini_int (ikey, r, cf[i], cf[i].type & 0xffff);
        break;

      case CONF_LOGFONT:
        reg2ini_logfont (ikey, r, cf[i]);
        break;

      case CONF_PRINT_FONT:
        reg2ini_print_font (ikey, r, cf[i]);
        break;
      }
}

static void
reg2ini_colors ()
{
  WCHAR *key = (WCHAR *)alloca ((wcslen (Registry::Settings) + wcslen (cfgColors) + 2)
                                * sizeof (WCHAR));
  wsprintfW (key, L"%s\\%s", Registry::Settings, cfgColors);

  ReadRegistry r (key);
  if (r.fail ())
    return;

  conf cf;
  cf.type = CONF_HEX;
  WCHAR name[16];
  cf.name = name;
  for (int i = 1; i <= 16; i++)
    {
      wsprintfW (name, L"%s%d", cfgFg, i);
      reg2ini_int (cfgColors, r, cf);
      wsprintfW (name, L"%s%d", cfgBg, i);
      reg2ini_int (cfgColors, r, cf);
    }

  COLORREF c[16];
  if (r.get (L"CustColors", c, sizeof c) == sizeof c)
    for (int i = 0; i < 16; i++)
      {
        wsprintfW (name, L"%s%d", cfgCustColor, i);
        write_conf (cfgColors, name, long (c[i]), 1);
      }
}

static void
reg2ini_geometry (const WCHAR *rkey)
{
  WCHAR *key = (WCHAR *)alloca ((wcslen (Registry::Settings) + wcslen (rkey) + 2)
                                * sizeof (WCHAR));
  wsprintfW (key, L"%s\\%s", Registry::Settings, rkey);
  EnumRegistry er (key);
  if (er.fail ())
    return;

  for (int i = 0;; i++)
    {
      WCHAR name[128];
      DWORD namel = numberof (name);
      WINDOWPLACEMENT w;
      DWORD wl = sizeof w;
      DWORD type;
      int e = RegEnumValueW (er, i, name, &namel, 0, &type, (BYTE *)&w, &wl);
      if (e == ERROR_SUCCESS)
        {
          if (type == REG_BINARY && wl == sizeof w && w.length == sizeof w)
            write_conf (rkey, name, w);
        }
      else if (e != ERROR_MORE_DATA)
        break;
    }
}

static void
reg2ini_geometry ()
{
  const WCHAR *rkey = cfgGeometry;
  WCHAR *key = (WCHAR *)alloca ((wcslen (Registry::Settings) + wcslen (rkey) + 2)
                                * sizeof (WCHAR));
  wsprintfW (key, L"%s\\%s", Registry::Settings, rkey);
  EnumRegistry er (key);
  if (er.fail ())
    return;

  for (int i = 0;; i++)
    {
      WCHAR name[128];
      DWORD namel = numberof (name);
      FILETIME ft;
      int e = RegEnumKeyExW (er, i, name, &namel, 0, 0, 0, &ft);
      if (e == ERROR_SUCCESS)
        {
          WINDOWPLACEMENT w;
          WCHAR key[256];
          wsprintfW (key, L"%s\\%s\\%s", Registry::Settings, cfgGeometry, name);
          ReadRegistry r (key);
          if (!r.fail ()
              && r.get (cfgShowCmd, (int *)&w.showCmd)
              && r.get (cfgLeft, &w.rcNormalPosition.left)
              && r.get (cfgTop, &w.rcNormalPosition.top)
              && r.get (cfgRight, &w.rcNormalPosition.right)
              && r.get (cfgBottom, &w.rcNormalPosition.bottom))
            write_conf (cfgMisc, name, w);
        }
      else if (e != ERROR_MORE_DATA)
        break;
    }

  reg2ini_geometry (cfgFiler);
  reg2ini_geometry (cfgPrintPreview);
}

int
reg2ini ()
{
  {
    ReadRegistry r (Registry::Settings);
    if (r.fail ())
      return 0;
  }

  reg2ini (L"", cfgMisc, misc, numberof (misc));
  reg2ini (cfgBufferSelector, 0, buffer_selector, numberof (buffer_selector));
  reg2ini (cfgColors, 0, colors, numberof (colors));
  reg2ini_colors ();
  reg2ini (cfgFiler, 0, filer, numberof (filer));
  reg2ini (cfgFont, 0, font, numberof (font));
  reg2ini (cfgPrint, 0, print, numberof (print));
  reg2ini (cfgPrintPreview, 0, preview, numberof (preview));
  reg2ini_geometry ();
  flush_conf ();
  return 1;
}

static int
reg_empty_tree_p (HKEY hkey)
{
  WCHAR cls[1024];
  DWORD clsl = numberof (cls);
  DWORD nkeys, keyl, xclsl, nvals, naml, datal, desc;
  FILETIME ft;
  if (RegQueryInfoKeyW (hkey, cls, &clsl, 0, &nkeys, &keyl, &xclsl,
                        &nvals, &naml, &datal, &desc, &ft) != ERROR_SUCCESS)
    return 0;
  return !(nkeys + nvals);
}

static int
delete_sub_tree (HKEY hkey, const WCHAR *name)
{
  {
    EnumRegistry r (hkey, name);
    if (!r.fail ())
      {
        for (int i = 0; i < 100; i++)
          {
            FILETIME ft;
            WCHAR buf[256];
            DWORD sz = numberof (buf);
            if (RegEnumKeyExW (r, 0, buf, &sz, 0, 0, 0, &ft) != ERROR_SUCCESS
                || !delete_sub_tree (r, buf))
              break;
          }
      }
  }
  return RegDeleteKeyW (hkey, name) == ERROR_SUCCESS;
}

void
reg_delete_tree ()
{
  {
    EnumRegistry r (HKEY_CURRENT_USER, L"Software\\Free Software");
    if (r.fail ())
      return;
    if (sysdep.WinNTp ())
      delete_sub_tree (r, L"xyzzy");
    else
      RegDeleteKeyW (r, L"xyzzy");
    if (!reg_empty_tree_p (r))
      return;
  }

  EnumRegistry r (HKEY_CURRENT_USER, L"Software");
  if (!r.fail ())
    RegDeleteKeyW (r, L"Free Software");
}
