#include "stdafx.h"
#include "ed.h"
#include "ChooseFont.h"

ChooseFontP::ChooseFontP ()
{
  cf_hil = dpi_scale_imagelist (app.hinst,
                                MAKEINTRESOURCE (IDB_TT),
                                18, RGB (0, 0, 255));
}

ChooseFontP::~ChooseFontP ()
{
  if (cf_hil)
    ImageList_Destroy (cf_hil);
}

// 枠ごとの一覧。指定が無い枠は、実際に使うことになるフォントを添える
void
ChooseFontP::update_slot_list (HWND hwnd)
{
  HWND list = GetDlgItem (hwnd, IDC_SLOTLIST);
  int sel = SendMessage (list, LB_GETCURSEL, 0, 0);
  SendMessage (list, WM_SETREDRAW, 0, 0);
  SendMessage (list, LB_RESETCONTENT, 0, 0);
  HDC hdc = GetDC (hwnd);
  for (int i = 0; i < FONT_MAX; i++)
    {
      WCHAR name[128], fmt[128], face[128], buf[256];
      *name = 0;
      LoadStringW (app.hinst, FontSet::lang_id (i), name, numberof (name));

      LOGFONTW lf;
      FontSet::resolve_logfont (lf, cf_param, i);
      if (*cf_param.fs_logfont[i].lfFaceName)
        wcscpy (face, lf.lfFaceName);
      else
        {
          *fmt = 0;
          LoadStringW (app.hinst, IDS_FONT_UNSPECIFIED, fmt, numberof (fmt));
          _snwprintf_s (face, numberof (face), _TRUNCATE, fmt, lf.lfFaceName);
        }

      // 導入されていないフォントは、指定しても別のもので描かれる
      if (!font_exist_p (hdc, lf.lfFaceName, lf.lfCharSet))
        {
          WCHAR shown[128];
          wcscpy (shown, face);
          *fmt = 0;
          LoadStringW (app.hinst, IDS_FONT_MISSING, fmt, numberof (fmt));
          _snwprintf_s (face, numberof (face), _TRUNCATE, fmt, shown);
        }

      _snwprintf_s (buf, numberof (buf), _TRUNCATE, L"%s\t%s", name, face);
      int idx = SendMessageW (list, LB_ADDSTRING, 0, LPARAM (buf));
      SendMessage (list, LB_SETITEMDATA, idx, i);
    }
  ReleaseDC (hwnd, hdc);
  SendMessage (list, LB_SETCURSEL, sel == LB_ERR ? 0 : sel, 0);
  SendMessage (list, WM_SETREDRAW, 1, 0);
  InvalidateRect (list, 0, 0);
}

void
ChooseFontP::add_slot_list (HWND hwnd)
{
  // タブ位置はリストボックスの文字幅を単位とする。DPI には追随しなくてよい
  int tab = 28;
  SendDlgItemMessage (hwnd, IDC_SLOTLIST, LB_SETTABSTOPS, 1, LPARAM (&tab));
  update_slot_list (hwnd);
}

int CALLBACK
ChooseFontP::enum_font_name_proc (ENUMLOGFONTW *elf, NEWTEXTMETRICW *, int type, LPARAM lparam)
{
  if (*elf->elfLogFont.lfFaceName != '@'
      && (elf->elfLogFont.lfPitchAndFamily & 3) == FIXED_PITCH)
    {
      HWND hwnd = HWND (lparam);
      if (SendMessageW (hwnd, LB_FINDSTRINGEXACT, WPARAM (-1),
                        LPARAM (elf->elfLogFont.lfFaceName)) == LB_ERR)
        {
          int i = SendMessageW (hwnd, LB_ADDSTRING, 0,
                                LPARAM (elf->elfLogFont.lfFaceName));
          SendMessage (hwnd, LB_SETITEMDATA, i, (elf->elfLogFont.lfCharSet << 8) | type);
        }
    }
  return 1;
}

void
ChooseFontP::add_font_name (HWND hwnd, HDC hdc)
{
  EnumFontFamiliesExW (hdc, 0, FONTENUMPROCW (enum_font_name_proc),
                       LPARAM (GetDlgItem (hwnd, IDC_NAMELIST)), 0);
}

int CALLBACK
ChooseFontP::enum_font_size_proc (ENUMLOGFONTW *elf, NEWTEXTMETRICW *, int type, LPARAM lparam)
{
  HWND hwnd = ((xdpi *)lparam)->hwnd;
  int dpi = ((xdpi *)lparam)->dpi;
  int pixel = ((xdpi *)lparam)->pixel;
  WCHAR b[16];
  if (type & TRUETYPE_FONTTYPE)
    {
      if (!pixel)
        {
          static const int tt[] =
            {6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 26, 28, 36,};
          if (SendMessageW (hwnd, LB_FINDSTRINGEXACT, WPARAM (-1), LPARAM (L"  6")) == LB_ERR)
            for (int i = 0; i < numberof (tt); i++)
              {
                wsprintfW (b, L"%3d", tt[i]);
                SendMessageW (hwnd, LB_ADDSTRING, 0, LPARAM (b));
              }
        }
      else
        {
          int min_pixel = FontObject::min_size_pixel ();
          int max_pixel = FontObject::max_size_pixel ();
          wsprintfW (b, L"%3d", min_pixel);
          if (SendMessageW (hwnd, LB_FINDSTRINGEXACT, WPARAM (-1), LPARAM (b)) == LB_ERR)
            for (int i = min_pixel; i <= max_pixel; i++)
              {
                wsprintfW (b, L"%3d", i);
                SendMessageW (hwnd, LB_ADDSTRING, 0, LPARAM (b));
              }
        }
    }
  else
    {
      wsprintfW (b, L"%3d", (pixel ? elf->elfLogFont.lfHeight
                             : MulDiv (elf->elfLogFont.lfHeight, 72, dpi)));
      if (SendMessageW (hwnd, LB_FINDSTRINGEXACT,
                        WPARAM (-1), LPARAM (b)) == LB_ERR)
        SendMessageW (hwnd, LB_ADDSTRING, 0, LPARAM (b));
    }
  return 1;
}

void
ChooseFontP::add_font_size (HWND hwnd, int i)
{
  WCHAR face[LF_FACESIZE];
  if (SendDlgItemMessageW (hwnd, IDC_NAMELIST, LB_GETTEXT, i, LPARAM (face)) == LB_ERR)
    return;
  SendDlgItemMessage (hwnd, IDC_SIZELIST, WM_SETREDRAW, 0, 0);
  SendDlgItemMessage (hwnd, IDC_SIZELIST, LB_RESETCONTENT, 0, 0);

  xdpi x;
  x.hwnd = GetDlgItem (hwnd, IDC_SIZELIST);
  x.dpi = cf_dpi;
  x.pixel = cf_param.fs_size_pixel;

  HDC hdc = GetDC (hwnd);
  EnumFontFamiliesW (hdc, face, FONTENUMPROCW (enum_font_size_proc), LPARAM (&x));
  ReleaseDC (hwnd, hdc);

  SendDlgItemMessage (hwnd, IDC_SIZELIST, WM_SETREDRAW, 1, 0);
  InvalidateRect (GetDlgItem (hwnd, IDC_SIZELIST), 0, 0);
}

void
ChooseFontP::change_font_size (HWND hwnd, int size)
{
  int i = SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_GETCURSEL, 0, 0);
  if (i == LB_ERR)
    return;

  add_font_size (hwnd, i);

  struct {int index, point;} min, max;
  min.index = max.index = -1;

  WCHAR b[16];
  int n = SendDlgItemMessage (hwnd, IDC_SIZELIST, LB_GETCOUNT, 0, 0);
  for (i = 0; i < n; i++)
    if (SendDlgItemMessageW (hwnd, IDC_SIZELIST, LB_GETTEXT, i, LPARAM (b)) != LB_ERR)
      {
        int x = _wtoi (b);
        if (x <= size && (min.index == -1 || x > min.point))
          {
            min.index = i;
            min.point = x;
          }
        if (x >= size && (max.index == -1 || x < max.point))
          {
            max.index = i;
            max.point = x;
          }
      }

  SendDlgItemMessage (hwnd, IDC_SIZELIST, LB_SETCURSEL,
                      ((min.index == -1 && max.index == -1)
                       ? 0
                       : (min.index == -1
                          ? max.index
                          : (max.index == -1
                             ? min.index
                             : (size - min.point <= max.point - size
                                ? min.index : max.index)))),
                      0);

  notify_font_size (hwnd, LBN_SELCHANGE);
}

void
ChooseFontP::select_primary (HWND hwnd)
{
  int j = SendDlgItemMessageW (hwnd, IDC_NAMELIST, LB_FINDSTRINGEXACT,
                               WPARAM (-1), LPARAM (cf_param.fs_primary.lfFaceName));
  if (j == LB_ERR)
    j = 0;
  SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_SETCURSEL, j, 0);

  change_font_size (hwnd,
                    (cf_param.fs_size_pixel
                     ? cf_param.fs_primary.lfHeight
                     : MulDiv (cf_param.fs_primary.lfHeight, 72, cf_dpi)));
}

void
ChooseFontP::notify_font_name (HWND hwnd, int code)
{
  if (code != LBN_SELCHANGE)
    return;
  int i = SendDlgItemMessage (hwnd, IDC_SIZELIST, LB_GETCURSEL, 0, 0);
  if (i == LB_ERR)
    return;
  WCHAR b[16];
  if (SendDlgItemMessageW (hwnd, IDC_SIZELIST, LB_GETTEXT, i, LPARAM (b)) == LB_ERR)
    return;
  change_font_size (hwnd, _wtoi (b));
}

void
ChooseFontP::notify_font_size (HWND hwnd, int code)
{
  if (code != LBN_SELCHANGE)
    return;

  int i = SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_GETCURSEL, 0, 0);
  if (i == LB_ERR)
    return;
  WCHAR name[LF_FACESIZE];
  if (SendDlgItemMessageW (hwnd, IDC_NAMELIST, LB_GETTEXT, i, LPARAM (name)) == LB_ERR)
    return;

  int j = SendDlgItemMessage (hwnd, IDC_SIZELIST, LB_GETCURSEL, 0, 0);
  if (j == LB_ERR)
    return;
  WCHAR b[16];
  if (SendDlgItemMessageW (hwnd, IDC_SIZELIST, LB_GETTEXT, j, LPARAM (b)) == LB_ERR)
    return;

  BYTE charset = BYTE (SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_GETITEMDATA, i, 0) >> 8);
  LOGFONTW lf;
  bzero (&lf, sizeof lf);
  lf.lfHeight = cf_param.fs_size_pixel ? _wtoi (b) : MulDiv (_wtoi (b), cf_dpi, 72);
  lf.lfCharSet = charset;
  wcscpy (lf.lfFaceName, name);

  cf_param.fs_primary = lf;
  update_slot_list (hwnd);

  HFONT hfont = CreateFontIndirectW (&lf);
  HFONT hfdlg = HFONT (SendMessage (hwnd, WM_GETFONT, 0, 0));
  HFONT hfctl = HFONT (SendDlgItemMessage (hwnd, IDC_SAMPLE, WM_GETFONT, 0, 0));
  SendDlgItemMessage (hwnd, IDC_SAMPLE, WM_SETFONT, WPARAM (hfont), MAKELPARAM (0, 0));
  if (hfctl != hfdlg)
    DeleteObject (hfctl);
  InvalidateRect (GetDlgItem (hwnd, IDC_SAMPLE), 0, 0);
}

void
ChooseFontP::notify_size_pixel (HWND hwnd, int code)
{
  if (code != BN_CLICKED)
    return;
  int i = SendDlgItemMessage (hwnd, IDC_SIZE_PIXEL, BM_GETCHECK, 0, 0);
  if (i != cf_param.fs_size_pixel)
    {
      cf_param.fs_size_pixel = i;

      int i = SendDlgItemMessage (hwnd, IDC_SIZELIST, LB_GETCURSEL, 0, 0);
      if (i == LB_ERR)
        return;
      WCHAR b[16];
      if (SendDlgItemMessageW (hwnd, IDC_SIZELIST, LB_GETTEXT, i, LPARAM (b)) == LB_ERR)
        return;
      int sz = _wtoi (b);
      if (cf_param.fs_size_pixel)
        sz = MulDiv (sz, cf_dpi, 72);
      else
        sz = MulDiv (sz, 72, cf_dpi);
      change_font_size (hwnd, sz);
    }
}

void
ChooseFontP::draw_font_list (HWND, DRAWITEMSTRUCT *dis)
{
  COLORREF ofg, obg;

  const int enabled = IsWindowEnabled (dis->hwndItem);
  if (dis->itemState & ODS_SELECTED && enabled)
    {
      ofg = SetTextColor (dis->hDC, sysdep.highlight_text);
      obg = SetBkColor (dis->hDC, sysdep.highlight);
    }
  else
    {
      ofg = SetTextColor (dis->hDC, enabled ? sysdep.window_text : sysdep.gray_text);
      obg = SetBkColor (dis->hDC, sysdep.window);
    }

  const RECT &r = dis->rcItem;
  if (dis->itemID != UINT (-1))
    {
      WCHAR b[LF_FACESIZE];
      *b = 0;
      SendMessageW (dis->hwndItem, LB_GETTEXT, dis->itemID, LPARAM (b));

      SIZE size;
      GetTextExtentPoint32W (dis->hDC, L"0", 1, &size);

      ExtTextOutW (dis->hDC, r.left + dpi_scale (18), (r.top + r.bottom - size.cy) / 2,
                   ETO_OPAQUE, &r, b, wcslen (b), 0);

      if (dis->itemData & TRUETYPE_FONTTYPE)
        ImageList_Draw (cf_hil, 0, dis->hDC,
                        r.left, (r.top + r.bottom - dpi_scale (12)) / 2, ILD_TRANSPARENT);
    }

  if (dis->itemState & ODS_FOCUS)
    DrawFocusRect (dis->hDC, &r);
  SetTextColor (dis->hDC, ofg);
  SetBkColor (dis->hDC, obg);
}

/* 見本はその文字集合のバイト列。組の文字集合を持つフォントで描くので、
   バイト列のまま渡す */
static const struct {BYTE charset; const char *string;} samples[] =
{
  {0, "AaBbCcXxYyZz"},
  {SHIFTJIS_CHARSET, "Aa\x82\xa0\x82\x9f\x83\x41\x83\x40\x88\x9f\x89\x46"},
  {CHINESEBIG5_CHARSET, "Aa\xa4\x40\xa4\x41\xc9\x40\xc9\x41"},
  {GB2312_CHARSET, "AaBb\xb0\xa1\xb0\xa2"},
  {HANGEUL_CHARSET, "Aa\xb0\xa1\xb0\xa2\xca\xa1\xca\xa2"},
  {HEBREW_CHARSET, "AaBb\xe0\xe1\xf9\xfa"},
  {ARABIC_CHARSET, "AaBb\xc7\xc8\xe7\xe8"},
  {GREEK_CHARSET, "AaBb\xc1\xe1\xc2\xe2"},
  {TURKISH_CHARSET, "AaBb\xc0\xe0\xde\xfe\xdf"},
  {RUSSIAN_CHARSET, "AaBb\xc0\xe0\xdf\xff"},
  {BALTIC_CHARSET, "AaBb\xc0\xe0\xdd\xfd"},
};

void
ChooseFontP::draw_sample (HWND hwnd, DRAWITEMSTRUCT *dis)
{
  const char *sample = samples[0].string;
  int i = SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_GETCURSEL, 0, 0);
  if (i != LB_ERR)
    {
      BYTE charset = BYTE (SendDlgItemMessage (hwnd, IDC_NAMELIST,
                                               LB_GETITEMDATA, i, 0) >> 8);
      for (int i = 0; i < numberof (samples); i++)
        if (charset == samples[i].charset)
          {
            sample = samples[i].string;
            break;
          }
    }

  HFONT hf = HFONT (SendMessage (dis->hwndItem, WM_GETFONT, 0, 0));
  HGDIOBJ of = SelectObject (dis->hDC, hf);
  COLORREF ofg = SetTextColor (dis->hDC, cf_fg);
  COLORREF obg = SetBkColor (dis->hDC, cf_bg);
  int l = strlen (sample);
  SIZE size = {0};
  GetTextExtentPoint32A (dis->hDC, sample, l, &size);
  const RECT &r = dis->rcItem;
  ExtTextOutA (dis->hDC, (r.left + r.right - size.cx) / 2,
               (r.top + r.bottom - size.cy) / 2,
               ETO_CLIPPED | ETO_OPAQUE, &r, sample, l, 0);

  SetTextColor (dis->hDC, ofg);
  SetBkColor (dis->hDC, obg);
  SelectObject (dis->hDC, of);

  paint_button_on (dis->hDC, r);
}

// 枠ごとのフォントを選ぶ。指定を外して代表フォントに任せることもできる
struct slot_font_arg
{
  ChooseFontP *cf;
  int specified;
  LOGFONTW lf;
};

INT_PTR CALLBACK
ChooseFontP::slot_font_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  slot_font_arg *arg = (slot_font_arg *)GetWindowLongPtrW (hwnd, DWLP_USER);

  switch (msg)
    {
    case WM_INITDIALOG:
      {
        arg = (slot_font_arg *)lparam;
        SetWindowLongPtrW (hwnd, DWLP_USER, LONG_PTR (arg));

        HDC hdc = GetDC (hwnd);
        arg->cf->add_font_name (hwnd, hdc);
        ReleaseDC (hwnd, hdc);

        SendDlgItemMessage (hwnd, IDC_SLOT_DEFAULT, BM_SETCHECK,
                            arg->specified ? 0 : 1, 0);
        int i = LB_ERR;
        if (arg->specified)
          i = SendDlgItemMessageW (hwnd, IDC_NAMELIST, LB_FINDSTRINGEXACT,
                                   WPARAM (-1), LPARAM (arg->lf.lfFaceName));
        SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_SETCURSEL, i == LB_ERR ? 0 : i, 0);
        EnableWindow (GetDlgItem (hwnd, IDC_NAMELIST), arg->specified);
        return 1;
      }

    case WM_MEASUREITEM:
      // 行の高さは、フォント名に添える画像より低くしない
      ((MEASUREITEMSTRUCT *)lparam)->itemHeight = max (get_font_height (hwnd),
                                                       dpi_scale (18));
      return 1;

    case WM_DRAWITEM:
      if (wparam != IDC_NAMELIST || !arg)
        return 0;
      arg->cf->draw_font_list (hwnd, (DRAWITEMSTRUCT *)lparam);
      return 1;

    case WM_COMMAND:
      if (!arg)
        return 0;
      switch (LOWORD (wparam))
        {
        case IDC_SLOT_DEFAULT:
          {
            HWND list = GetDlgItem (hwnd, IDC_NAMELIST);
            EnableWindow (list,
                          !SendDlgItemMessage (hwnd, IDC_SLOT_DEFAULT, BM_GETCHECK, 0, 0));
            InvalidateRect (list, 0, 0);
          }
          return 1;

        case IDOK:
          if (SendDlgItemMessage (hwnd, IDC_SLOT_DEFAULT, BM_GETCHECK, 0, 0))
            *arg->lf.lfFaceName = 0;
          else
            {
              int i = SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_GETCURSEL, 0, 0);
              if (i == LB_ERR)
                return 1;
              WCHAR face[LF_FACESIZE];
              if (SendDlgItemMessageW (hwnd, IDC_NAMELIST, LB_GETTEXT,
                                       i, LPARAM (face)) == LB_ERR)
                return 1;
              wcscpy (arg->lf.lfFaceName, face);
              arg->lf.lfCharSet =
                BYTE (SendDlgItemMessage (hwnd, IDC_NAMELIST, LB_GETITEMDATA, i, 0) >> 8);
            }
          EndDialog (hwnd, IDOK);
          return 1;

        case IDCANCEL:
          EndDialog (hwnd, IDCANCEL);
          return 1;
        }
      return 0;

    default:
      return 0;
    }
}

void
ChooseFontP::notify_set_slot_font (HWND hwnd)
{
  int i = SendDlgItemMessage (hwnd, IDC_SLOTLIST, LB_GETCURSEL, 0, 0);
  if (i == LB_ERR)
    return;
  int slot = SendDlgItemMessage (hwnd, IDC_SLOTLIST, LB_GETITEMDATA, i, 0);
  if (slot < 0 || slot >= FONT_MAX)
    return;

  slot_font_arg arg;
  arg.cf = this;
  arg.specified = *cf_param.fs_logfont[slot].lfFaceName != 0;
  // 指定が無かった枠に面を与えるときは、大きさを代表フォントから引き継ぐ
  arg.lf = arg.specified ? cf_param.fs_logfont[slot] : cf_param.fs_primary;

  if (DialogBoxParamW (app.hinst, MAKEINTRESOURCEW (IDD_SLOT_FONT), hwnd,
                       slot_font_proc, LPARAM (&arg)) != IDOK)
    return;

  cf_param.fs_logfont[slot] = arg.lf;
  update_slot_list (hwnd);
}

int
ChooseFontP::draw_item (HWND hwnd, int id, DRAWITEMSTRUCT *dis)
{
  switch (id)
    {
    case IDC_NAMELIST:
      draw_font_list (hwnd, dis);
      return 1;

    case IDC_SAMPLE:
      draw_sample (hwnd, dis);
      return 1;

    default:
      return 0;
    }
}

void
ChooseFontP::init_dialog (HWND hwnd)
{
  HDC hdc = GetDC (hwnd);
  cf_dpi = GetDeviceCaps (hdc, LOGPIXELSY);
  add_font_name (hwnd, hdc);
  ReleaseDC (hwnd, hdc);

  SendDlgItemMessage (hwnd, IDC_SIZE_PIXEL, BM_SETCHECK,
                      cf_param.fs_size_pixel ? 1 : 0, 0);

  add_slot_list (hwnd);
  select_primary (hwnd);
}

int
ChooseFontP::do_command (HWND hwnd, int id, int code)
{
  switch (id)
    {
    case IDC_SET_SLOT_FONT:
      if (code == BN_CLICKED)
        notify_set_slot_font (hwnd);
      return 1;

    case IDC_SLOTLIST:
      if (code == LBN_DBLCLK)
        notify_set_slot_font (hwnd);
      return 1;

    case IDC_NAMELIST:
      notify_font_name (hwnd, code);
      return 1;

    case IDC_SIZELIST:
      notify_font_size (hwnd, code);
      return 1;

    case IDC_SIZE_PIXEL:
      notify_size_pixel (hwnd, code);
      return 1;

    default:
      return 0;
    }
}

void
ChooseFontP::do_destroy (HWND hwnd)
{
  HFONT hfdlg = HFONT (SendMessage (hwnd, WM_GETFONT, 0, 0));
  HFONT hfctl = HFONT (SendDlgItemMessage (hwnd, IDC_SAMPLE, WM_GETFONT, 0, 0));
  if (hfctl != hfdlg)
    DeleteObject (hfctl);
}

void
ChooseFontP::set_color (HWND hwnd, COLORREF fg, COLORREF bg)
{
  cf_fg = fg;
  cf_bg = bg;
  InvalidateRect (GetDlgItem (hwnd, IDC_SAMPLE), 0, 0);
}
