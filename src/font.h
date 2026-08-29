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
  LOGFONTW fo_logfont;
public:
  FontObject () : fo_hfont (0) {}
  ~FontObject () {if (fo_hfont) DeleteObject (fo_hfont);}
  int create (const LOGFONTW &);
  int create (const WCHAR *, int, int);
  operator HFONT () const {return fo_hfont;}
  const HFONT hfont () const {return fo_hfont;}
  void get_metrics ();
  void get_metrics (HDC);
  void calc_offset (const SIZE &);
  const SIZE &size () const {return fo_size;}
  const POINT &offset () const {return fo_offset;}
  int ascent () const {return fo_ascent;}
  const LOGFONTW &logfont () const {return fo_logfont;}
  static const bool update (LOGFONTW &lf, const lisp keys, const bool recommend_size_p);
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

// フォントの枠。符号位置だけでは描くフォントが決まらないものにだけ枠を与える。
// 漢字は同じ符号位置に言語ごとの字形があり、ハングルはフォントリンクで出る字形の
// 幅が GetCharWidth32W に現れない。それ以外の用字は欧文の枠が受け持つ
#define FONT_ASCII          0   // 欧文。ラテン・キリル・ギリシャ・ジョージアなど
#define FONT_JP             1
#define FONT_CN_SIMPLIFIED  2
#define FONT_CN_TRADITIONAL 3
#define FONT_HANGUL         4
#define FONT_MAX            5

// 半角の升目に並べている文字を、フォントが全角の字形しか持たないときにどう扱うか
enum
{
  AMBIGUOUS_WIDTH_AUTO,   // フォントに合わせて二桁にする
  AMBIGUOUS_WIDTH_HALF    // 一桁のまま、字形をフォント側で押し込む
};

struct FontSetParam
{
  // 代表フォント。枠に指定が無ければここから決める
  LOGFONTW fs_primary;
  // 枠ごとの上書き。面が空なら指定が無い
  LOGFONTW fs_logfont[FONT_MAX];
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
  void measure_wide_glyphs () const;
  void update_char_columns () const;

  static const UINT fs_lang_id[];
  static const lisp *const fs_lang_key[];
  static const WCHAR *const fs_regent[];
  // 用字ごとにフォントを分けていた頃の鍵を、今の枠へ写す
  struct langalias {const lisp *key; int slot;};
  static const langalias fs_lang_alias[];
  // disp が入っていなければ alt を使う。Windows の版によって、入っている
  // フォントが違うため
  struct fontface {const WCHAR *disp, *alt, *print; int charset;};
  static const fontface fs_default_face[];
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
  LOGFONTW fs_primary;
  // 指定されたとおりの枠ごとの上書き。面が空なら指定が無い
  LOGFONTW fs_logfont[FONT_MAX];
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
  const LOGFONTW &primary () const {return fs_primary;}
  const LOGFONTW &slot_logfont (int n) const {return fs_logfont[n];}
  int specified_p (int n) const {return *fs_logfont[n].lfFaceName != 0;}
  // 指定の無い枠に使うフォントを決める。代表フォントがその枠の字形を持っていれば
  // 代表を、持っていなければ枠ごとの既定のフォントを充てる
  static void resolve_logfont (LOGFONTW &lf, const FontSetParam &param, int slot);
  const HBITMAP &hbm () const {return fs_hbm;}
  const SIZE &size () const {return fs_size;}
  const SIZE &cell () const {return fs_cell;}
  int use_backsl_p () const {return fs_use_backsl;}
  int line_width () const {return fs_line_width;}
  int line_spacing () const {return fs_line_spacing;}
  int recommend_size_p () const {return fs_recommend_size;}
  int size_pixel_p () const {return fs_size_pixel;}
  int ambiguous_width () const {return fs_ambiguous_width;}

  static const WCHAR *regent (int n) {return fs_regent[n];}
  static const WCHAR *default_face (int n, int print);
  static int default_charset (int n) {return fs_default_face[n].charset;}
  static UINT lang_id (int n) {return fs_lang_id[n];}
  static const lisp lang_key (int n) {return *fs_lang_key[n];}
  static int lang_key_index (lisp llang);
};

// 符号位置を、それを描くフォントの枠へ対応づける。漢字はどの言語の字形で描くか
// が符号位置だけでは決まらないので、バッファが持つ言語を渡す
int font_slot_of (Char cc, int lang = ENCODING_LANG_NIL);

// BMP 外の文字を描くフォント。字送りが二桁に収まるよう高さを抑える
HFONT create_surrogate_font (const SIZE &cell);

// 置換文字を描くフォント。字送りが一桁に収まるよう高さを抑える
HFONT create_replacement_font (const SIZE &cell);

int get_font_height (HWND hwnd);
bool font_exist_p (const HDC hdc, const WCHAR *face, BYTE charset);

// フォントの寸法を記録する節の名前。画面の DPI ごとに分かれる
const WCHAR *font_conf_section ();
// 上の節から LOGFONTW を読む。無ければ [Font] 節を読んで DPI に合わせて換算する
int read_font_conf (const WCHAR *name, LOGFONTW &lf);

#endif /* _font_h_ */
