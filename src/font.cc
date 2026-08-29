#include "stdafx.h"
#include "ed.h"
#include "conf.h"

const UINT FontSet::fs_lang_id[] =
{
  IDS_LANG_WESTERN,
  IDS_LANG_JAPANESE,
  IDS_LANG_CN_SIMPLIFIED,
  IDS_LANG_CN_TRADITIONAL,
  IDS_LANG_HANGUL,
};

const lisp *const FontSet::fs_lang_key[] =
{
  &Kascii,
  &Kjapanese,
  &Kcn_simplified,
  &Kcn_traditional,
  &Khangul,
};

// 枠ごとの設定を記録する ini のキー。文字集合の名前で書かれてきたものを引き継ぐ
const WCHAR *const FontSet::fs_regent[] =
{
  L"Ascii",
  L"Japanese",
  L"GB2312",
  L"BIG5",
  L"KSC5601",
};

// 用字ごとにフォントを分けていた頃の鍵。欧文の枠が受け持つようになった
const FontSet::langalias FontSet::fs_lang_alias[] =
{
  {&Klatin,    FONT_ASCII},
  {&Kcyrillic, FONT_ASCII},
  {&Kgreek,    FONT_ASCII},
  {&Kgeorgian, FONT_ASCII},
  {&Kksc5601,  FONT_HANGUL},
};

// ＭＳ ゴシックは漢字とハングルの字形を持たないものについても全角の幅を答えるので、
// フォントリンクで出る字形が升目からはみ出さない。字形を持つフォントが無いときの受け皿
const FontSet::fontface FontSet::fs_default_face[] =
{
  {L"BIZ UDGothic", L"ＭＳ ゴシック", 0, SHIFTJIS_CHARSET},
  {L"BIZ UDGothic", L"ＭＳ ゴシック", 0, SHIFTJIS_CHARSET},
  {L"Microsoft YaHei", L"SimSun", 0, GB2312_CHARSET},
  {L"Microsoft JhengHei", L"ＭＳ ゴシック", 0, CHINESEBIG5_CHARSET},
  {L"Malgun Gothic", L"ＭＳ ゴシック", 0, HANGEUL_CHARSET},
};

// 挙げたどれも入っていないときの受け皿
static const WCHAR fallback_face[] = L"ＭＳ ゴシック";

// 挙げた順に、実際に入っているものを使う
const WCHAR *
FontSet::default_face (int n, int print)
{
  const fontface &f = fs_default_face[n];
  if (print && f.print)
    return f.print;
  if (!f.alt)
    return f.disp;
  HDC hdc = GetDC (0);
  const WCHAR *face = f.disp;
  if (!font_exist_p (hdc, f.disp, f.charset))
    face = font_exist_p (hdc, f.alt, f.charset) ? f.alt : fallback_face;
  ReleaseDC (0, hdc);
  return face;
}

// 枠が受け持つ字形をフォントが持っているかを見るための符号位置
static const WCHAR slot_probe[FONT_MAX][3] =
{
  {0, 0, 0},                    // 欧文は代表フォントそのものが受け持つ
  {0x3042, 0x6f22, 0xff21},     // 仮名・漢字・全角形
  {0x4e2d, 0x6c49, 0},          // 簡体字
  {0x4e2d, 0x3105, 0},          // 繁体字と注音符号
  {0xd55c, 0x3131, 0xffa1},     // ハングルの音節・互換字母・半角形
};

static bool
font_has_glyphs_p (HDC hdc, const LOGFONTW &lf, const WCHAR *probe)
{
  HFONT hfont = CreateFontIndirectW (&lf);
  if (!hfont)
    return false;
  HGDIOBJ of = SelectObject (hdc, hfont);
  bool covers = true;
  for (int i = 0; i < 3 && probe[i]; i++)
    {
      WORD gi;
      if (GetGlyphIndicesW (hdc, probe + i, 1, &gi, GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR
          || gi == 0xffff)
        {
          covers = false;
          break;
        }
    }
  SelectObject (hdc, of);
  DeleteObject (hfont);
  return covers;
}

// フォントリンクで代わりに描かれる字形の送り幅は GetCharWidth32W に現れないので、
// 字形を持たないフォントを枠に充てると升目が崩れる。代表フォントを充てるのは
// その枠の字形を持っているときだけにする
void
FontSet::resolve_logfont (LOGFONTW &lf, const FontSetParam &param, int slot)
{
  if (*param.fs_logfont[slot].lfFaceName)
    {
      lf = param.fs_logfont[slot];
      return;
    }

  lf = param.fs_primary;
  if (slot == FONT_ASCII)
    return;

  HDC hdc = GetDC (0);
  const bool covers = font_has_glyphs_p (hdc, lf, slot_probe[slot]);
  ReleaseDC (0, hdc);
  if (covers)
    return;

  wcscpy (lf.lfFaceName, default_face (slot, 0));
  lf.lfCharSet = BYTE (default_charset (slot));
}

int
FontSet::lang_key_index (lisp llang)
{
  for (int i = 0; i < FONT_MAX; i++)
    if (lang_key (i) == llang)
      return i;
  for (int i = 0; i < numberof (fs_lang_alias); i++)
    if (*fs_lang_alias[i].key == llang)
      return fs_lang_alias[i].slot;
  return -1;
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

// ラテン補助に混じっている記号。CP932 にもあり、日本語のフォントでは全角に
// 作られている。ラテン文字と同じ枠に入れると半角の字形で描かれてしまう
static int
latin_symbol_p (Char cc)
{
  switch (cc)
    {
    case 0x00a7:  // §
    case 0x00a8:  // ¨
    case 0x00b0:  // °
    case 0x00b1:  // ±
    case 0x00b4:  // ´
    case 0x00b6:  // ¶
    case 0x00d7:  // ×
    case 0x00f7:  // ÷
      return 1;

    default:
      return 0;
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
    return latin_symbol_p (cc) ? FONT_JP : FONT_ASCII;
  if (cc < 0x0370)              // IPA 拡張・修飾文字・結合分音記号
    return FONT_JP;
  if (cc < 0x0530)              // ギリシャ・キリル
    return FONT_ASCII;
  if (cc >= 0x10a0 && cc < 0x1100)  // ジョージア
    return FONT_ASCII;
  if (cc >= 0x1e00 && cc < 0x2000)  // ラテン拡張追加・ギリシャ拡張
    return FONT_ASCII;

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
  LOGFONTW lf;
  bzero (&lf, sizeof lf);
  lf.lfHeight = min (long (cell.cy), cell.cx * 2);
  lf.lfCharSet = DEFAULT_CHARSET;
  wcscpy (lf.lfFaceName, L"Segoe UI Emoji");
  return CreateFontIndirectW (&lf);
}

// 日本語のフォントは U+FFFD の字形を持たない。フォントリンクで拾われる字形は
// 環境によって中黒と見分けの付かない点になるので、字形を持つフォントを名指しする。
// 高さに対して字幅がおよそ 5/8 なので、一桁に収まるようその比で抑える
HFONT
create_replacement_font (const SIZE &cell)
{
  LOGFONTW lf;
  bzero (&lf, sizeof lf);
  lf.lfHeight = min (long (cell.cy), cell.cx * 8 / 5);
  lf.lfCharSet = DEFAULT_CHARSET;
  wcscpy (lf.lfFaceName, L"Microsoft Sans Serif");
  return CreateFontIndirectW (&lf);
}

int
FontObject::create (const WCHAR *face, int h, int charset)
{
  LOGFONTW lf;
  bzero (&lf, sizeof lf);
  wcscpy (lf.lfFaceName, face);
  lf.lfHeight = h;
  lf.lfCharSet = charset;
  lf.lfPitchAndFamily = FIXED_PITCH;
  return create (lf);
}

int
FontObject::create (const LOGFONTW &lf)
{
  HFONT h = CreateFontIndirectW (&lf);
  if (!h)
    return 0;
  if (fo_hfont)
    DeleteObject (fo_hfont);
  fo_hfont = h;
  GetObjectW (h, sizeof fo_logfont, &fo_logfont);
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

void
FontObject::calc_offset (const SIZE &sz)
{
  fo_offset.x = (sz.cx - fo_size.cx) / 2;
  fo_offset.y = (sz.cy - fo_size.cy) / 2;
}

const bool
FontObject::update (LOGFONTW &lf, const lisp keys, const bool recommend_size_p)
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

  // :default は枠の指定を外し、代表フォントに任せることを表す
  if (lface == Kdefault)
    {
      if (*lf.lfFaceName)
        {
          *lf.lfFaceName = 0;
          update = true;
        }
    }
  else if (lface != Qnil)
    {
      check_string (lface);
      WCHAR *face = (WCHAR *)alloca (sizeof (WCHAR) * (w2ul (lface) + 1));
      w2u (face, lface);
      if (wcscmp (lf.lfFaceName, face) != 0)
        {
          wcscpy (lf.lfFaceName, face);
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

  TextOutW (hdc, fs_cell.cx * backsl, 0, L"/", 1);
  StretchBlt (hdc, fs_cell.cx * backsl, 0, fs_cell.cx, fs_cell.cy,
              hdc, fs_cell.cx * (backsl + 1) - 1, 0, -fs_cell.cx, fs_cell.cy,
              SRCCOPY);

  TextOutW (hdc, fs_cell.cx * bold_backsl, 0, L"/", 1);
  int omode = SetBkMode (hdc, TRANSPARENT);
  TextOutW (hdc, fs_cell.cx * bold_backsl + 1, 0, L"/", 1);
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
  WCHAR c = '<';
  ExtTextOutW (hdc, m0 + f.offset ().x, f.offset ().y, 0, 0, &c, 1, 0);
  ExtTextOutW (hdc, m1 + f.offset ().x, f.offset ().y, 0, 0, &c, 1, 0);
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

  fs_primary = param.fs_primary;
  LOGFONTW logfont[FONT_MAX];
  for (int i = 0; i < FONT_MAX; i++)
    {
      fs_logfont[i] = param.fs_logfont[i];
      resolve_logfont (logfont[i], param, i);
    }

  if (!fs_recommend_size)
    {
      for (int i = 0; i < FONT_MAX; i++)
        fs_font[i].create (logfont[i]);

      for (int i = 0; i < FONT_MAX; i++)
        fs_font[i].get_metrics (hdc);
    }
  else
    {
      fs_font[FONT_ASCII].create (logfont[FONT_ASCII]);
      fs_font[FONT_ASCII].get_metrics (hdc);

      for (int i = 1; i < FONT_MAX; i++)
        for (int h = fs_font[FONT_ASCII].size ().cy; h > 0; h--)
          {
            LOGFONTW lf (logfont[i]);
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
        LOGFONTW lf (logfont[i]);
        lf.lfWidth = fs_size.cx;
        fs_font[i].create (lf);
        fs_font[i].get_metrics (hdc);
      }

  ReleaseDC (0, hdc);

  fs_cell.cx = fs_size.cx;
  fs_cell.cy = fs_size.cy + fs_line_spacing;
  fs_ascent = fs_font[FONT_JP].ascent ();
  fs_line_width = fs_size.cy / 12;
  if (!fs_line_width)
    fs_line_width = 1;

  for (int i = 0; i < FONT_MAX; i++)
    fs_font[i].calc_offset (fs_size);

  measure_wide_glyphs ();
  update_char_columns ();
  create_bitmap ();
  save_params (param);
  return 1;
}

// 字形が升目に収まらないか。送り幅が升目の一倍半を超えていれば全角とみなす
static inline int
wide_glyph_width_p (int cx, int cellw)
{
  return cx * 2 >= cellw * 3;
}

// 幅を測る相手か。ふたつの升目を占める字はそもそも全角で、専用のフォントで描く
// 字は担当のフォントでは測れない
static int
measurable_p (Char cc)
{
  return (charset_width (cc) == 1
          && !utf16_surrogate_high_p (cc) && !utf16_surrogate_low_p (cc)
          && !(cc >= UNICODE_SMLCDM_MIN && cc <= UNICODE_SMLCDM_MAX)
          && cc != UNICODE_REPLACEMENT_CHAR);
}

// 担当のフォントが、一桁の升目に置く字を全角の字形で描くかを字ごとに測る。
// 日本語のフォントは半角と全角の字形を混ぜて持つので、文字体系の単位では決まらない。
// フォント自身が字形を持たないものは測らない。GDI がフォントリンクで選んだ別の
// フォントの寸法になり、このフォントの幅にならないため
void
FontSet::measure_wide_glyphs () const
{
  memset (char_wide_glyph_table, 0, sizeof char_wide_glyph_table);
  if (fs_size.cx <= 0)
    return;

  enum {max_run = 512};
  WCHAR w[max_run];
  WORD gi[max_run];
  INT cx[max_run];

  HDC hdc = GetDC (0);
  for (Char cc = 0x80; cc < CHAR_LIMIT;)
    {
      if (!measurable_p (cc))
        {
          cc++;
          continue;
        }

      const Char from = cc;
      const int slot = font_slot_of (cc);
      int n = 0;
      while (n < max_run && cc < CHAR_LIMIT
             && measurable_p (cc) && font_slot_of (cc) == slot)
        w[n++] = WCHAR (cc++);

      HGDIOBJ of = SelectObject (hdc, fs_font[slot]);
      if (GetGlyphIndicesW (hdc, w, n, gi, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR
          && GetCharWidth32W (hdc, from, from + n - 1, cx))
        for (int i = 0; i < n; i++)
          if (gi[i] != 0xffff && wide_glyph_width_p (cx[i], fs_size.cx))
            char_wide_glyph_table[w[i] >> 3] |= 1 << (w[i] & 7);
      SelectObject (hdc, of);
    }
  ReleaseDC (0, hdc);
}

// 半角の升目に並べている文字体系。日本語のフォントではこれらも全角に作られて
// いるため、「曖昧な文字幅を半角に固定」はこの枠にだけ効かせる
static int
half_width_slot_p (int slot)
{
  return slot == FONT_ASCII;
}

// 一桁の升目に置く字を担当のフォントが全角で描くなら、二桁として扱う。
// 一桁に押し込むと字形の右半分が切れて読めなくなる
void
FontSet::update_char_columns () const
{
  memcpy (char_columns_table, char_width_table, sizeof char_width_table);
  for (Char cc = 0x80; cc < CHAR_LIMIT; cc++)
    if (wide_glyph_p (cc)
        && (fs_ambiguous_width == AMBIGUOUS_WIDTH_AUTO
            || !half_width_slot_p (font_slot_of (cc))))
      char_columns_table[cc >> 3] |= 1 << (cc & 7);
}

// フォントの寸法はピクセルで記録されるため画面の DPI に依存する。DPI ごとに
// 別の節へ記録し、その節が無ければ従来の [Font] 節を BASE_SCREEN_DPI のものと
// みなして換算する。DPI に依存しない設定は [Font] 節に置いたままにする。
const WCHAR *
font_conf_section ()
{
  static WCHAR section[32];
  if (!*section)
    _snwprintf_s (section, numberof (section), _TRUNCATE,
                  L"%s@%d", cfgFont, screen_dpi ());
  return section;
}

int
read_font_conf (const WCHAR *name, LOGFONTW &lf)
{
  /* DPI ごとの節に記載があれば、面が読めなくてもそれが答え。指定の無い枠は面が
     空で記録されるので、ここで [Font] 節へ落とすと古い記載が生き返り、しかも
     DPI に合わせて換算されて大きさが変わる */
  WCHAR buf[128];
  if (read_conf (font_conf_section (), name, buf, numberof (buf)))
    return read_conf (font_conf_section (), name, lf);
  if (!read_conf (cfgFont, name, lf))
    return 0;
  lf.lfHeight = dpi_scale (lf.lfHeight);
  return 1;
}

static int
read_font_conf (const WCHAR *name, int &value)
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
  write_conf (font_conf_section (), cfgPrimaryFont, param.fs_primary);
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
fix_charset_proc (ENUMLOGFONTW *elf, NEWTEXTMETRICW *, int type, LPARAM lparam)
{
  HDC hdc = GetDC (0);
  FontSetParam &param = *(FontSetParam *)lparam;
  if (*elf->elfLogFont.lfFaceName != '@')
    for (int i = -1; i < FONT_MAX; i++)
      {
        LOGFONTW &lf = i < 0 ? param.fs_primary : param.fs_logfont[i];
        if (!*lf.lfFaceName)
          continue;
        if (font_exist_p (hdc, lf.lfFaceName, lf.lfCharSet))
          continue;
        if (!wcscmp (elf->elfLogFont.lfFaceName, lf.lfFaceName))
          lf.lfCharSet = elf->elfLogFont.lfCharSet;
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

  // 代表フォントの記録が無ければ、欧文の枠に書かれているものを引き継ぐ。用字ごとに
  // フォントを分けていた頃の設定は、これで見た目が変わらないまま読める
  const bool inherited = !read_font_conf (cfgPrimaryFont, param.fs_primary);
  if (inherited)
    {
      if (*param.fs_logfont[FONT_ASCII].lfFaceName)
        param.fs_primary = param.fs_logfont[FONT_ASCII];
      else
        {
          LOGFONTW lf;
          GetObjectW (GetStockObject (SYSTEM_FIXED_FONT), sizeof lf, &lf);
          wcscpy (param.fs_primary.lfFaceName, default_face (FONT_ASCII, 0));
          param.fs_primary.lfCharSet = BYTE (default_charset (FONT_ASCII));
          param.fs_primary.lfHeight = dpi_scale (lf.lfHeight);
        }

      // 代表フォントと同じものを指している枠は、指定を外して代表に従わせる。用字
      // ごとにフォントを分けていた頃の設定は五つの枠が全て埋まっているため、その
      // ままでは代表フォントを変えても何も追随しない。同じフォントなので見た目は
      // 変わらない。大きさは、お任せのときは枠ごとに決め直すので見ない
      for (int i = 0; i < FONT_MAX; i++)
        if (*param.fs_logfont[i].lfFaceName
            && !wcscmp (param.fs_logfont[i].lfFaceName, param.fs_primary.lfFaceName)
            && param.fs_logfont[i].lfCharSet == param.fs_primary.lfCharSet
            && (param.fs_recommend_size
                || param.fs_logfont[i].lfHeight == param.fs_primary.lfHeight))
          *param.fs_logfont[i].lfFaceName = 0;
    }

  for (int i = -1; i < FONT_MAX; i++)
    {
      LOGFONTW &lf = i < 0 ? param.fs_primary : param.fs_logfont[i];
      if (!*lf.lfFaceName)
        continue;
      lf.lfPitchAndFamily &= ~3;
      lf.lfPitchAndFamily |= FIXED_PITCH;
    }

  HDC hdc = GetDC (0);
  EnumFontFamiliesExW (hdc, 0, FONTENUMPROCW (fix_charset_proc), LPARAM (&param), 0);
  ReleaseDC (0, hdc);
}

void
FontSet::init ()
{
  FontSetParam param;
  load_params (param);
  create (param);
}

// 枠ごとの面は、指定が無い枠については実際に使っているものを返す
lisp
FontSet::make_alist () const
{
  lisp r = Qnil;
  for (int i = -1; i < FONT_MAX; i++)
    {
      LOGFONTW lf = i < 0 ? primary () : font (i).logfont ();
      int size = lf.lfHeight;
      if (!size_pixel_p ())
        size = FontObject::pixel_to_point (size);
      r = xcons (make_list (i < 0 ? Kdefault : FontSet::lang_key (i),
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
  param.fs_primary = primary ();
  for (int i = 0; i < FONT_MAX; i++)
    param.fs_logfont[i] = slot_logfont (i);

  // Update FontSetParam.fs_logfont by lfontset;
  bool update = false;
  for (lisp x = lfontset; consp (x); x = xcdr (x))
    {
      check_cons (xcar (x));
      lisp llang = Fcaar (x);
      lisp keys = Fcdar (x);

      if (llang == Kdefault)
        {
          // 代表フォントは必ず面を持つ。指定を外せるのは枠だけ
          if (find_keyword (Kface, keys) == Kdefault)
            FEtype_error (Kdefault, Qstring);
          if (FontObject::update (param.fs_primary, keys, false))
            update = true;
          continue;
        }

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
check_valid_font (const ENUMLOGFONTW *, const NEWTEXTMETRICW *,
                  DWORD, LPARAM lparam)
{
  *(bool *)lparam = true;
  return 0;
}

bool
font_exist_p (const HDC hdc, const WCHAR *face, BYTE charset)
{
  bool exists = false;

  LOGFONTW font;
  memset (&font, 0, sizeof font);
  font.lfCharSet = charset;
  wcscpy (font.lfFaceName, face);

  EnumFontFamiliesExW (hdc, &font,
                       FONTENUMPROCW (check_valid_font),
                       LPARAM (&exists), 0);

  return exists;
}
