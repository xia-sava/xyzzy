#ifndef _ChooseFont_h_
#define _ChooseFont_h_

class ChooseFontP
{
protected:
  int cf_dpi;
  HIMAGELIST cf_hil;

public:
  FontSetParam cf_param;
  COLORREF cf_fg, cf_bg;

protected:
  struct xdpi
    {
      int dpi;
      int pixel;
      HWND hwnd;
    };

  static int CALLBACK enum_font_name_proc (ENUMLOGFONTW *, NEWTEXTMETRICW *, int, LPARAM);
  static int CALLBACK enum_font_size_proc (ENUMLOGFONTW *, NEWTEXTMETRICW *, int, LPARAM);
  static INT_PTR CALLBACK slot_font_proc (HWND, UINT, WPARAM, LPARAM);
  void add_font_name (HWND, HDC);
  void reload_font_name (HWND);
  void add_font_size (HWND, int);
  void change_font_size (HWND, int);
  void select_primary (HWND);
  void add_slot_list (HWND);
  void update_slot_list (HWND);
  void notify_font_name (HWND, int);
  void notify_font_size (HWND, int);
  void notify_size_pixel (HWND, int);
  void notify_proportional (HWND);
  void notify_set_slot_font (HWND);
  void draw_font_list (HWND, DRAWITEMSTRUCT *);
  void draw_sample (HWND, DRAWITEMSTRUCT *);

public:
  ChooseFontP ();
  ~ChooseFontP ();
  void init_dialog (HWND);
  int do_command (HWND, int, int);
  void do_destroy (HWND);
  int draw_item (HWND, int, DRAWITEMSTRUCT *);
  void set_color (HWND, COLORREF, COLORREF);
};

#endif /* _ChooseFont_h_ */
