#include "stdafx.h"
#include "ed.h"
#include "conf.h"

const UINT FontSet::fs_lang_id[] =
{
  IDS_LANG_ASCII,
  IDS_LANG_JAPANESE,
  IDS_LANG_LATIN,
  IDS_LANG_CYRILLIC,
  IDS_LANG_GREEK,
  IDS_LANG_CN_SIMPLIFIED,
  IDS_LANG_CN_TRADITIONAL,
  IDS_LANG_KSC5601,
  IDS_LANG_GEORGIAN,
};

const lisp *const FontSet::fs_lang_key[] =
{
  &Kascii,
  &Kjapanese,
  &Klatin,
  &Kcyrillic,
  &Kgreek,
  &Kcn_simplified,
  &Kcn_traditional,
  &Kksc5601,
  &Kgeorgian,
};

const char *const FontSet::fs_regent[] =
{
  "Ascii",
  "Japanese",
  "Latin",
  "Cyrillic",
  "Greek",
  "GB2312",
  "BIG5",
  "KSC5601",
  "Georgian",
};

// 各スロットの送り幅を測るための文字。受け持つ文字体系の中から、その体系を
// 収めたフォントならまず持っている字を選ぶ
const ucs2_t FontSet::fs_sample_char[] =
{
  'A',
  0x3042,  // あ
  0x00e0,  // à
  0x0430,  // а
  0x03b1,  // α
  0x4e2d,  // 中
  0x4e2d,  // 中
  0xac00,  // 가
  0x10d0,  // ა
};

const FontSet::fontface FontSet::fs_default_face[] =
{
  {"BIZ UDGothic", "ＭＳ ゴシック", 0, SHIFTJIS_CHARSET},
  {"BIZ UDGothic", "ＭＳ ゴシック", 0, SHIFTJIS_CHARSET},
  {"Courier New", 0},
  {"Courier New", 0},
  {"Courier New", 0},
  {"Microsoft YaHei", "SimSun", 0, GB2312_CHARSET},
  {"Microsoft JhengHei", "MingLiu", 0, CHINESEBIG5_CHARSET},
  {"Malgun Gothic", "GulimChe", 0, HANGEUL_CHARSET},
  {"BPG Courier New U", "Sylfaen"},
};

// 挙げた順に、実際に入っているものを使う
const char *
FontSet::default_face (int n, int print)
{
  const fontface &f = fs_default_face[n];
  if (print && f.print)
    return f.print;
  if (!f.alt)
    return f.disp;
  HDC hdc = GetDC (0);
  const char *face = font_exist_p (hdc, f.disp, f.charset) ? f.disp : f.alt;
  ReleaseDC (0, hdc);
  return face;
}

// 漢字は同じ字が日本・中国・台湾・韓国で共通の符号位置になる。どの字形で描くかは
// 符号位置だけでは決められないので、バッファが持つ言語で振り分ける
static int
han_font_slot (int lang)
{
  switch (lang)
    {
    case ENCODING_LANG_KR:
      return FONT_HANGUL;

    case ENCODING_LANG_CN:
    case ENCODING_LANG_CN_GB:
      return FONT_CN_SIMPLIFIED;

    case ENCODING_LANG_CN_BIG5:
      return FONT_CN_TRADITIONAL;

    default:
      return FONT_JP;
    }
}

// 符号位置がどの文字体系に属するかでフォントの枠を決める。画面・印刷・入力の
// いずれもここを通す
int
font_slot_of (Char cc, int lang)
{
  if (cc < 0x80)
    return FONT_ASCII;

  if (cc < 0x0250)              // ラテン補助・ラテン拡張 A/B
    return FONT_LATIN;
  if (cc < 0x0370)              // IPA 拡張・修飾文字・結合分音記号
    return FONT_JP;
  if (cc < 0x0400)              // ギリシャ
    return FONT_GREEK;
  if (cc < 0x0530)              // キリル
    return FONT_CYRILLIC;
  if (cc >= 0x10a0 && cc < 0x1100)
    return FONT_GEORGIAN;
  if (cc >= 0x1e00 && cc < 0x1f00)  // ラテン拡張追加
    return FONT_LATIN;
  if (cc >= 0x1f00 && cc < 0x2000)  // ギリシャ拡張
    return FONT_GREEK;

  // ハングル。字母・互換字母・音節
  if (cc >= 0x1100 && cc < 0x1200
      || cc >= 0x3130 && cc < 0x3190
      || cc >= 0xac00 && cc < 0xd7b0
      || cc >= 0xffa0 && cc < 0xffdd)
    return FONT_HANGUL;

  // 漢字。部首・康熙部首・統合漢字・互換漢字
  if (cc >= 0x2e80 && cc < 0x2fe0
      || cc >= 0x3005 && cc < 0x3006
      || cc >= 0x3400 && cc < 0x4dc0
      || cc >= 0x4e00 && cc < 0xa000
      || cc >= 0xf900 && cc < 0xfb00)
    return han_font_slot (lang);

  // 注音符号は繁体字の文字集合にしかない
  if (cc >= 0x3100 && cc < 0x3130 || cc >= 0x31a0 && cc < 0x31c0)
    return FONT_CN_TRADITIONAL;

  // 対にならなかったサロゲートは字が無い。豆腐は ASCII のフォントで出す
  if (utf16_surrogate_high_p (cc) || utf16_surrogate_low_p (cc))
    return FONT_ASCII;

  // 仮名・約物・全角形など、日本語のフォントが受け持つもの
  return FONT_JP;
}

// 幅は指定しない。指定すると、フォントリンクで選ばれた代替フォントにも平均文字幅と
// して掛かり、全角を基準に持つ漢字のフォントが倍の幅に引き伸ばされる
HFONT
create_surrogate_font (const SIZE &cell)
{
  LOGFONT lf;
  bzero (&lf, sizeof lf);
  lf.lfHeight = min (long (cell.cy), cell.cx * 2);
  lf.lfCharSet = DEFAULT_CHARSET;
  strcpy (lf.lfFaceName, "Segoe UI Emoji");
  return CreateFontIndirect (&lf);
}

int
FontObject::create (const char *face, int h, int charset)
{
  LOGFONT lf;
  bzero (&lf, sizeof lf);
  strcpy (lf.lfFaceName, face);
  lf.lfHeight = h;
  lf.lfCharSet = charset;
  lf.lfPitchAndFamily = FIXED_PITCH;
  return create (lf);
}

int
FontObject::create (const LOGFONT &lf)
{
  HFONT h = CreateFontIndirect (&lf);
  if (!h)
    return 0;
  if (fo_hfont)
    DeleteObject (fo_hfont);
  fo_hfont = h;
  GetObject (h, sizeof fo_logfont, &fo_logfont);
  return 1;
}

void
FontObject::get_metrics ()
{
  HDC hdc = GetDC (0);
  get_metrics (hdc);
  ReleaseDC (0, hdc);
}

void
FontObject::get_metrics (HDC hdc)
{
  HGDIOBJ of = SelectObject (hdc, fo_hfont);
  TEXTMETRIC tm;
  GetTextMetrics (hdc, &tm);
  fo_size.cx = tm.tmAveCharWidth;
  fo_size.cy = tm.tmAscent + tm.tmDescent;
  fo_ascent = tm.tmAscent;
  SelectObject (hdc, of);
}

// 受け持つ文字体系の代表の字を実測して、升目いくつぶんの送りになるかを決める。
// フォント自身がその字を持っていないときは測らない。GDI がフォントリンクで
// 選んだ別のフォントの寸法になり、このフォントの幅にならないため
void
FontObject::measure_columns (HDC hdc, ucs2_t sample, int cellw)
{
  fo_columns = 1;
  if (cellw <= 0)
    return;

  HGDIOBJ of = SelectObject (hdc, fo_hfont);
  WORD gi;
  SIZE sz;
  if (GetGlyphIndicesW (hdc, LPCWSTR (&sample), 1, &gi,
                        GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR
      && gi != 0xffff
      && GetTextExtentPoint32W (hdc, LPCWSTR (&sample), 1, &sz))
    fo_columns = min (2, max (1, int ((sz.cx * 2 + cellw) / (cellw * 2))));
  SelectObject (hdc, of);
}

void
FontObject::calc_offset (const SIZE &sz)
{
  fo_offset.x = (sz.cx - fo_size.cx) / 2;
  fo_offset.y = (sz.cy - fo_size.cy) / 2;
}

const bool
FontObject::update (LOGFONT &lf, const lisp keys, const bool recommend_size_p)
{
  check_cons (keys);
  lisp lface = find_keyword (Kface, keys);
  lisp lsize = find_keyword (Ksize, keys);

  bool update = false;
  if (lsize != Qnil && !recommend_size_p)
    {
      int size = fixnum_value (lsize);
      int old_size;
      int pixel;
      if (find_keyword_bool (Ksize_pixel_p, keys))
        {
          old_size =lf.lfHeight;
          pixel = size;
        }
      else
        {
          old_size = FontObject::pixel_to_point (lf.lfHeight);
          pixel = FontObject::point_to_pixel (size);
        }
      if (pixel < FontObject::min_size_pixel ()
          || pixel > FontObject::max_size_pixel ())
        FErange_error (lsize);
      if (old_size != size)
        {
          lf.lfHeight = pixel;
          lf.lfWidth = 0;
          update = true;
        }
    }

  if (lface != Qnil)
    {
      check_string (lface);
      char *face = (char *)alloca (xstring_length (lface) * 2 + 1);
      w2s (face, lface);
      if (strcmp (lf.lfFaceName, face) != 0)
        {
          strcpy (lf.lfFaceName, face);
          update = true;
        }
    }

  return update;
}

void
FontSet::paint_newline_bitmap (HDC hdc)
{
  int h = fs_size.cy / 2;
  int y0 = fs_size.cy - 2;
  int ox = fs_cell.cx * newline + 2;
  int y;
  for (y = 0; y < h; y++)
    SetPixel (hdc, ox, y0 - y, RGB (0, 0, 0));
  for (y = 0; y < h / 2 - 1; y++)
    SetPixel (hdc, ox + y, y0 - y, RGB (0, 0, 0));
  int w, x;
  for (w = (y + 1) / 2, x = y; x >= w; x--)
    SetPixel (hdc, ox + x, y0 - y, RGB (0, 0, 0));
  for (x++; y < h; y++)
    SetPixel (hdc, ox + x, y0 - y, RGB (0, 0, 0));
  for (y--; x >= 0; x--)
    SetPixel (hdc, ox + x, y0 - y, RGB (0, 0, 0));
}

void
FontSet::paint_backsl_bitmap (HDC hdc)
{
  HGDIOBJ of = SelectObject (hdc, fs_font[FONT_ASCII]);

  TextOut (hdc, fs_cell.cx * backsl, 0, "/", 1);
  StretchBlt (hdc, fs_cell.cx * backsl, 0, fs_cell.cx, fs_cell.cy,
              hdc, fs_cell.cx * (backsl + 1) - 1, 0, -fs_cell.cx, fs_cell.cy,
              SRCCOPY);

  TextOut (hdc, fs_cell.cx * bold_backsl, 0, "/", 1);
  int omode = SetBkMode (hdc, TRANSPARENT);
  TextOut (hdc, fs_cell.cx * bold_backsl + 1, 0, "/", 1);
  SetBkMode (hdc, omode);
  StretchBlt (hdc, fs_cell.cx * bold_backsl, 0, fs_cell.cx, fs_cell.cy,
              hdc, fs_cell.cx * (bold_backsl + 1) - 1, 0, -fs_cell.cx, fs_cell.cy,
              SRCCOPY);

  SelectObject (hdc, of);
}

void
FontSet::paint_sep_bitmap (HDC hdc)
{
  int x = fs_cell.cx * sep + fs_cell.cx / 4;
  MoveToEx (hdc, x, 0, 0);
  LineTo (hdc, x, fs_cell.cy);
}

void
FontSet::paint_tab_bitmap (HDC hdc)
{
  int h = fs_ascent / 4;
  int x0 = fs_cell.cx * htab + (fs_cell.cx - h) / 2;
  int y0 = fs_ascent - 1;
  MoveToEx (hdc, x0, y0, 0);
  LineTo (hdc, x0 + h, y0);
  LineTo (hdc, x0, y0 - h);
  LineTo (hdc, x0, y0);
}

void
FontSet::paint_fullspc_bitmap (HDC hdc)
{
  int h = fs_ascent / 4;
  if (!h)
    h = 2;
  else if (h & 1)
    h++;
  int w = fs_size.cx * 2 * 3 / 4;
  if (!w)
    w = 2;
  else if (w & 1)
    w++;

  int x1 = fs_cell.cx * fullspc1 + (fs_size.cx * 2 - w) / 2;
  int x2 = x1 + w;
  int y1 = fs_ascent - 1;
  int y2 = fs_ascent - h;

  for (int x = x1; x < x2; x += 2)
    {
      SetPixel (hdc, x, y1, RGB (0, 0, 0));
      SetPixel (hdc, x + 1, y2, RGB (0, 0, 0));
    }
  x2--;
  for (int y = y1 - 2; y > y2; y -= 2)
    {
      SetPixel (hdc, x1, y, RGB (0, 0, 0));
      SetPixel (hdc, x2, y + 1, RGB (0, 0, 0));
    }
}

void
FontSet::paint_halfspc_bitmap (HDC hdc)
{
  int h = fs_size.cy / 5;
  if (h < 3)
    h = 3;

  MoveToEx (hdc, fs_size.cx * halfspc + 1, fs_ascent - h, 0);
  LineTo (hdc, fs_size.cx * halfspc + 1, fs_ascent - 1);
  LineTo (hdc, fs_size.cx * (halfspc + 1) - 2, fs_ascent - 1);
  LineTo (hdc, fs_size.cx * (halfspc + 1) - 2, fs_ascent - h - 1);
}

void
FontSet::paint_blank (HDC hdc)
{
  if (fs_size.cx > 2 && fs_ascent > 2)
    {
      PatBlt (hdc, fs_cell.cx * blank + 1, 1,
              fs_size.cx - 2, fs_ascent - 2, BLACKNESS);
      PatBlt (hdc, fs_cell.cx * wblank1 + 1, 1,
              fs_size.cx * 2 - 2, fs_ascent - 2, BLACKNESS);
    }
}

void
FontSet::paint_fold_bitmap (HDC hdc)
{
  int s0 = fs_cell.cx * fold_sep0;
  int s1 = fs_cell.cx * fold_sep1;
  int m0 = fs_cell.cx * fold_mark_sep0;
  int m1 = fs_cell.cx * fold_mark_sep1;

  PatBlt (hdc, s0, 0, fs_cell.cx, fs_cell.cy, WHITENESS);
  PatBlt (hdc, s1, 0, fs_cell.cx, fs_cell.cy, WHITENESS);
  PatBlt (hdc, m0, 0, fs_cell.cx, fs_cell.cy, WHITENESS);
  PatBlt (hdc, m1, 0, fs_cell.cx, fs_cell.cy, WHITENESS);

  const FontObject &f = fs_font[FONT_ASCII];
  HGDIOBJ of = SelectObject (hdc, f);
  char c = '<';
  ExtTextOut (hdc, m0 + f.offset ().x, f.offset ().y, 0, 0, &c, 1, 0);
  ExtTextOut (hdc, m1 + f.offset ().x, f.offset ().y, 0, 0, &c, 1, 0);
  SelectObject (hdc, of);

  for (int y = 0; y < fs_cell.cy; y += 2)
    {
      SetPixel (hdc, s0, y, RGB (0, 0, 0));
      SetPixel (hdc, m0, y, RGB (0, 0, 0));
    }
  for (int y = fs_cell.cy & 1; y < fs_cell.cy; y += 2)
    {
      SetPixel (hdc, s1, y, RGB (0, 0, 0));
      SetPixel (hdc, m1, y, RGB (0, 0, 0));
    }
}

void
FontSet::create_bitmap ()
{
  if (fs_hbm)
    DeleteObject (fs_hbm);
  fs_hbm = CreateBitmap (fs_cell.cx * max_bitmap, fs_cell.cy, 1, 1, 0);
  HDC hdc = GetDC (0);
  HDC hdcmem = CreateCompatibleDC (hdc);
  ReleaseDC (0, hdc);
  HGDIOBJ obm = SelectObject (hdcmem, fs_hbm);
  HGDIOBJ open = SelectObject (hdcmem, CreatePen (PS_SOLID, 0, RGB (0, 0, 0)));
  PatBlt (hdcmem, 0, 0, fs_cell.cx * max_bitmap, fs_cell.cy, WHITENESS);
  paint_newline_bitmap (hdcmem);
  paint_backsl_bitmap (hdcmem);
  paint_sep_bitmap (hdcmem);
  paint_tab_bitmap (hdcmem);
  paint_fullspc_bitmap (hdcmem);
  paint_halfspc_bitmap (hdcmem);
  paint_blank (hdcmem);
  paint_fold_bitmap (hdcmem);
  DeleteObject (SelectObject (hdcmem, open));
  SelectObject (hdcmem, obm);
  DeleteDC (hdcmem);
}

int
FontSet::create (const FontSetParam &param)
{
  HDC hdc = GetDC (0);

  fs_line_spacing = max (0, min (param.fs_line_spacing, dpi_scale (30)));
  fs_use_backsl = param.fs_use_backsl;
  fs_recommend_size = param.fs_recommend_size;
  fs_size_pixel = param.fs_size_pixel;
  fs_ambiguous_width = param.fs_ambiguous_width;

  if (!fs_recommend_size)
    {
      for (int i = 0; i < FONT_MAX; i++)
        fs_font[i].create (param.fs_logfont[i]);

      for (int i = 0; i < FONT_MAX; i++)
        fs_font[i].get_metrics (hdc);
    }
  else
    {
      fs_font[FONT_ASCII].create (param.fs_logfont[FONT_ASCII]);
      fs_font[FONT_ASCII].get_metrics (hdc);

      for (int i = 1; i < FONT_MAX; i++)
        for (int h = fs_font[FONT_ASCII].size ().cy; h > 0; h--)
          {
            LOGFONT lf (param.fs_logfont[i]);
            lf.lfHeight = h;
            lf.lfWidth = 0;
            fs_font[i].create (lf);
            fs_font[i].get_metrics (hdc);
            if (fs_font[i].size ().cx <= fs_font[FONT_ASCII].size ().cx)
              break;
          }
    }

  fs_size = fs_font[FONT_ASCII].size ();

  for (int i = 0; i < FONT_MAX; i++)
    if (fs_font[i].size ().cx > fs_size.cx)
      {
        LOGFONT lf (param.fs_logfont[i]);
        lf.lfWidth = fs_size.cx;
        fs_font[i].create (lf);
        fs_font[i].get_metrics (hdc);
      }

  for (int i = 0; i < FONT_MAX; i++)
    fs_font[i].measure_columns (hdc, fs_sample_char[i], fs_size.cx);

  ReleaseDC (0, hdc);

  fs_cell.cx = fs_size.cx;
  fs_cell.cy = fs_size.cy + fs_line_spacing;
  fs_ascent = fs_font[FONT_JP].ascent ();
  fs_line_width = fs_size.cy / 12;
  if (!fs_line_width)
    fs_line_width = 1;

  for (int i = 0; i < FONT_MAX; i++)
    fs_font[i].calc_offset (fs_size);

  update_char_columns ();
  create_bitmap ();
  save_params (param);
  return 1;
}

// 半角の升目に並べているが、フォントによっては全角の字形しか持たない文字体系。
// ラテン・キリル・ギリシャ・グルジアは、日本語のフォントでは全角に作られている
static int
ambiguous_width_slot_p (int slot)
{
  return (slot == FONT_LATIN || slot == FONT_CYRILLIC
          || slot == FONT_GREEK || slot == FONT_GEORGIAN);
}

int
FontSet::full_width_slot_p (int slot) const
{
  return ambiguous_width_slot_p (slot) && fs_font[slot].columns () == 2;
}

int
FontSet::full_width_p (Char cc) const
{
  return charset_width (cc) == 1 && full_width_slot_p (font_slot_of (cc));
}

// 半角として並べている文字を、担当のフォントが全角で描くなら二桁として扱う。
// 一桁の升目に押し込むと字形の右半分が切れて読めなくなる
void
FontSet::update_char_columns () const
{
  memcpy (char_columns_table, char_width_table, sizeof char_width_table);
  if (fs_ambiguous_width != AMBIGUOUS_WIDTH_AUTO)
    return;

  for (int i = 0; i <= 0xffff; i++)
    if (full_width_p (Char (i)))
      char_columns_table[i >> 3] |= 1 << (i & 7);
}

// フォントの寸法はピクセルで記録されるため画面の DPI に依存する。DPI ごとに
// 別の節へ記録し、その節が無ければ従来の [Font] 節を BASE_SCREEN_DPI のものと
// みなして換算する。DPI に依存しない設定は [Font] 節に置いたままにする。
const char *
font_conf_section ()
{
  static char section[32];
  if (!*section)
    _snprintf_s (section, sizeof section, _TRUNCATE,
                 "%s@%d", cfgFont, screen_dpi ());
  return section;
}

int
read_font_conf (const char *name, LOGFONT &lf)
{
  if (read_conf (font_conf_section (), name, lf))
    return 1;
  if (!read_conf (cfgFont, name, lf))
    return 0;
  lf.lfHeight = dpi_scale (lf.lfHeight);
  return 1;
}

static int
read_font_conf (const char *name, int &value)
{
  if (read_conf (font_conf_section (), name, value))
    return 1;
  if (!read_conf (cfgFont, name, value))
    return 0;
  value = dpi_scale (value);
  return 1;
}

void
FontSet::save_params (const FontSetParam &param)
{
  for (int i = 0; i < FONT_MAX; i++)
    write_conf (font_conf_section (), regent (i), param.fs_logfont[i]);
  write_conf (font_conf_section (), cfgLineSpacing, param.fs_line_spacing);
  write_conf (cfgFont, cfgBackslash, param.fs_use_backsl);
  write_conf (cfgFont, cfgRecommendSize, param.fs_recommend_size);
  write_conf (cfgFont, cfgSizePixel, param.fs_size_pixel);
  write_conf (cfgFont, cfgAmbiguousWidth, param.fs_ambiguous_width);
  flush_conf ();
}

static int CALLBACK
fix_charset_proc (ENUMLOGFONT *elf, NEWTEXTMETRIC *, int type, LPARAM lparam)
{
  HDC hdc = GetDC (0);
  FontSetParam &param = *(FontSetParam *)lparam;
  if (*elf->elfLogFont.lfFaceName != '@')
    for (int i = 0; i < FONT_MAX; i++)
      {
        if (font_exist_p (hdc, param.fs_logfont[i].lfFaceName, param.fs_logfont[i].lfCharSet))
          continue;
        if (!strcmp (elf->elfLogFont.lfFaceName, param.fs_logfont[i].lfFaceName))
          param.fs_logfont[i].lfCharSet = elf->elfLogFont.lfCharSet;
      }
  ReleaseDC (0, hdc);
  return 1;
}

void
FontSet::load_params (FontSetParam &param)
{
  bzero (&param, sizeof param);

  if (!read_font_conf (cfgLineSpacing, param.fs_line_spacing))
    param.fs_line_spacing = 0;
  if (!read_conf (cfgFont, cfgBackslash, param.fs_use_backsl))
    param.fs_use_backsl = 0;
  if (!read_conf (cfgFont, cfgRecommendSize, param.fs_recommend_size))
    param.fs_recommend_size = 0;
  if (!read_conf (cfgFont, cfgSizePixel, param.fs_size_pixel))
    param.fs_size_pixel = 0;
  if (!read_conf (cfgFont, cfgAmbiguousWidth, param.fs_ambiguous_width))
    param.fs_ambiguous_width = AMBIGUOUS_WIDTH_AUTO;
  for (int i = 0; i < FONT_MAX; i++)
    if (!read_font_conf (regent (i), param.fs_logfont[i]))
      *param.fs_logfont[i].lfFaceName = 0;

  for (int i = 0; i < FONT_MAX; i++)
    {
      if (!*param.fs_logfont[i].lfFaceName)
        {
          strcpy (param.fs_logfont[i].lfFaceName, default_face (i, 0));
          if (!i)
            {
              LOGFONT lf;
              GetObject (GetStockObject (SYSTEM_FIXED_FONT), sizeof lf, &lf);
              param.fs_logfont[0].lfHeight = dpi_scale (lf.lfHeight);
            }
          else
            param.fs_logfont[i].lfHeight = param.fs_logfont[0].lfHeight;
        }
      param.fs_logfont[i].lfPitchAndFamily &= ~3;
      param.fs_logfont[i].lfPitchAndFamily |= FIXED_PITCH;
    }

  HDC hdc = GetDC (0);
  EnumFontFamiliesEx (hdc, 0, FONTENUMPROC (fix_charset_proc), LPARAM (&param), 0);
  ReleaseDC (0, hdc);
}

void
FontSet::init ()
{
  FontSetParam param;
  load_params (param);
  create (param);
}

lisp
FontSet::make_alist () const
{
  lisp r = Qnil;
  for (int i = 0; i < FONT_MAX; i++)
    {
      LOGFONT lf = font (i).logfont ();
      int size = lf.lfHeight;
      if (!size_pixel_p ())
        size = FontObject::pixel_to_point (size);
      r = xcons (make_list (FontSet::lang_key (i),
                            Kface, make_string (lf.lfFaceName),
                            Ksize, make_fixnum (size),
                            Ksize_pixel_p, boole (size_pixel_p ()),
                            0),
                 r);
    }

  return Fnreverse (r);
}

const bool
FontSet::update (FontSetParam &param, const lisp lfontset) const
{
  // Initialize FontSetParam by current setting.
  param.fs_use_backsl = use_backsl_p ();
  param.fs_line_spacing = line_spacing ();
  param.fs_recommend_size = recommend_size_p ();
  param.fs_size_pixel = size_pixel_p ();
  param.fs_ambiguous_width = ambiguous_width ();
  for (int i = 0; i < FONT_MAX; i++)
    param.fs_logfont[i] = font (i).logfont ();

  // Update FontSetParam.fs_logfont by lfontset;
  bool update = false;
  for (lisp x = lfontset; consp (x); x = xcdr (x))
    {
      check_cons (xcar (x));
      lisp llang = Fcaar (x);
      lisp keys = Fcdar (x);

      int n = FontSet::lang_key_index (llang);
      if (n < 0)
        FEsimple_error (Einvalid_charset, llang);

      if (FontObject::update (param.fs_logfont[n], keys, (llang != Kascii && recommend_size_p ())))
        update = true;
    }

  return update;
}

lisp
Fget_text_fontset ()
{
  return app.text_font.make_alist ();
}

lisp
Fset_text_fontset (lisp lfontset)
{
  check_cons (lfontset);

  FontSetParam param;
  if (!app.text_font.update (param, lfontset))
    return Qnil;

  Window::change_parameters (param);
  refresh_screen (0);

  return Qt;
}

int
get_font_height (HWND hwnd)
{
  HFONT hfont = HFONT (SendMessage (hwnd, WM_GETFONT, 0, 0));
  HDC hdc = GetDC (hwnd);
  HGDIOBJ ofont = SelectObject (hdc, hfont);
  TEXTMETRIC tm;
  GetTextMetrics (hdc, &tm);
  SelectObject (hdc, ofont);
  ReleaseDC (hwnd, hdc);
  return tm.tmHeight;
}

static int CALLBACK
check_valid_font (const ENUMLOGFONT *, const NEWTEXTMETRIC *,
                  DWORD, LPARAM lparam)
{
  *(bool *)lparam = true;
  return 0;
}

bool
font_exist_p (const HDC hdc, const char *face, BYTE charset)
{
  bool exists = false;

  LOGFONT font;
  memset (&font, 0, sizeof LOGFONT);
  font.lfCharSet = charset;
  strcpy (font.lfFaceName, face);

  EnumFontFamiliesEx (hdc, &font,
                      FONTENUMPROC (check_valid_font),
                      LPARAM (&exists), 0);

  return exists;
}
