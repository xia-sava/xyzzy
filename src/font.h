#ifndef _font_h_
#define _font_h_

// 選択できるフォントサイズの範囲。96 DPI では 8〜48 ピクセルに相当する
#define FONT_SIZE_MIN_POINT 6
#define FONT_SIZE_MAX_POINT 36

class FontObject
{
protected:
  HFONT fo_hfont;
  POINT fo_offset;
  SIZE fo_size;
  int fo_ascent;
  int fo_columns;
  LOGFONT fo_logfont;
public:
  FontObject () : fo_hfont (0), fo_columns (1) {}
  ~FontObject () {if (fo_hfont) DeleteObject (fo_hfont);}
  int create (const LOGFONT &);
  int create (const char *, int, int);
  operator HFONT () const {return fo_hfont;}
  const HFONT hfont () const {return fo_hfont;}
  void get_metrics ();
  void get_metrics (HDC);
  void measure_columns (HDC, ucs2_t sample, int cellw);
  void calc_offset (const SIZE &);
  const SIZE &size () const {return fo_size;}
  int columns () const {return fo_columns;}
  const POINT &offset () const {return fo_offset;}
  int ascent () const {return fo_ascent;}
  const LOGFONT &logfont () const {return fo_logfont;}
  static const bool update (LOGFONT &lf, const lisp keys, const bool recommend_size_p);
  static const int dpi ()
    {
      return screen_dpi ();
    }
  static const int pixel_to_point (int pixel)
    {
      return MulDiv (pixel, 72, dpi ());
    }
  static const int point_to_pixel (int point)
    {
      return MulDiv (point, dpi (), 72);
    }
  static const int min_size_pixel ()
    {
      return point_to_pixel (FONT_SIZE_MIN_POINT);
    }
  static const int max_size_pixel ()
    {
      return point_to_pixel (FONT_SIZE_MAX_POINT);
    }
};

#define FONT_ASCII          0
#define FONT_JP             1
#define FONT_LATIN          2
#define FONT_CYRILLIC       3
#define FONT_GREEK          4
#define FONT_CN_SIMPLIFIED  5
#define FONT_CN_TRADITIONAL 6
#define FONT_HANGUL         7
#define FONT_GEORGIAN       8
#define FONT_MAX            9

// 半角の升目に並べている文字を、フォントが全角の字形しか持たないときにどう扱うか
enum
{
  AMBIGUOUS_WIDTH_AUTO,   // フォントに合わせて二桁にする
  AMBIGUOUS_WIDTH_HALF    // 一桁のまま、字形をフォント側で押し込む
};

struct FontSetParam
{
  LOGFONT fs_logfont[FONT_MAX];
  int fs_use_backsl;
  int fs_line_spacing;
  int fs_recommend_size;
  int fs_size_pixel;
  int fs_ambiguous_width;
};

class FontSet
{
protected:
  void create_bitmap ();
  void paint_newline_bitmap (HDC);
  void paint_backsl_bitmap (HDC);
  void paint_tab_bitmap (HDC);
  void paint_fullspc_bitmap (HDC);
  void paint_halfspc_bitmap (HDC);
  void paint_sep_bitmap (HDC);
  void paint_blank (HDC);
  void paint_fold_bitmap (HDC);
  void save_params (const FontSetParam &);
  void load_params (FontSetParam &);
  void update_char_columns () const;

  static const UINT fs_lang_id[];
  static const lisp *const fs_lang_key[];
  static const char *const fs_regent[];
  struct fontface {const char *disp, *print; int charset;};
  static const fontface fs_default_face[];
  static const ucs2_t fs_sample_char[];
public:
  enum
    {
      backsl,
      newline,
      htab,
      fullspc1,
      fullspc2,
      sep,
      blank,
      wblank1,
      wblank2,
      halfspc,
      bold_backsl,
      fold_sep0,
      fold_sep1,
      fold_mark_sep0,
      fold_mark_sep1,
      max_bitmap
    };

protected:
  FontObject fs_font[FONT_MAX];
  HBITMAP fs_hbm;
  SIZE fs_size;
  SIZE fs_cell;
  int fs_ascent;
  int fs_line_spacing;
  int fs_use_backsl;
  int fs_line_width;
  int fs_recommend_size;
  int fs_size_pixel;
  int fs_ambiguous_width;

public:
  FontSet () : fs_hbm (0) {}
  ~FontSet () {if (fs_hbm) DeleteObject (fs_hbm);}
  int create (const FontSetParam &);
  void init ();
  lisp make_alist () const;
  const bool update (FontSetParam &param, const lisp lfontset) const;
  const FontObject &font (int n) const {return fs_font[n];}
  const HBITMAP &hbm () const {return fs_hbm;}
  const SIZE &size () const {return fs_size;}
  const SIZE &cell () const {return fs_cell;}
  int use_backsl_p () const {return fs_use_backsl;}
  int line_width () const {return fs_line_width;}
  int line_spacing () const {return fs_line_spacing;}
  int recommend_size_p () const {return fs_recommend_size;}
  int size_pixel_p () const {return fs_size_pixel;}
  int ambiguous_width () const {return fs_ambiguous_width;}
  // 半角として並べている文字を、担当のフォントが全角の字形で描くか
  int full_width_p (Char cc) const;

  static const char *regent (int n) {return fs_regent[n];}
  static const char *default_face (int n, int print)
    {return (!print || !fs_default_face[n].print
             ? fs_default_face[n].disp : fs_default_face[n].print);}
  static int default_charset (int n) {return fs_default_face[n].charset;}
  static UINT lang_id (int n) {return fs_lang_id[n];}
  static const lisp lang_key (int n) {return *fs_lang_key[n];}
  static const int lang_key_index (lisp llang)
    {
      for (int i = 0; i < FONT_MAX; i++)
        {
          if (lang_key (i) == llang)
            return i;
        }
      return -1;
    }
};

// 内部コードを、それを描くフォントの枠へ対応づける。画面・印刷・入力のいずれも
// ここを通す
int font_slot_of (Char cc);

int get_font_height (HWND hwnd);
bool font_exist_p (const HDC hdc, const char *face, BYTE charset);

// フォントの寸法を記録する節の名前。画面の DPI ごとに分かれる
const char *font_conf_section ();
// 上の節から LOGFONT を読む。無ければ [Font] 節を読んで DPI に合わせて換算する
int read_font_conf (const char *name, LOGFONT &lf);

#endif /* _font_h_ */
