#include "stdafx.h"
#include "ed.h"

void
Window::mc_clear ()
{
  w_nmcursors = 0;
  w_mc_direction = 0;
}

/* 居た範囲を描き直させてから消す */
void
Window::mc_discard ()
{
  if (!w_nmcursors)
    return;
  w_bufp->set_modified_region (w_mcursors[0].mc_point,
                               w_mcursors[w_nmcursors - 1].mc_point + 1);
  mc_clear ();
}

/* POINT に居るカーソルの添字。居なければ挿入位置の補数を返す */
int
Window::mc_search (point_t point) const
{
  int lo = 0, hi = w_nmcursors - 1;
  while (lo <= hi)
    {
      int mid = (lo + hi) / 2;
      point_t p = w_mcursors[mid].mc_point;
      if (p == point)
        return mid;
      if (p < point)
        lo = mid + 1;
      else
        hi = mid - 1;
    }
  return ~lo;
}

int
Window::mc_add (point_t point, point_t mark)
{
  int i = mc_search (point);
  if (i >= 0 || w_nmcursors >= MC_MAX_CURSORS)
    return 0;
  i = ~i;

  if (w_nmcursors == w_mcursors_size)
    {
      int size = w_mcursors_size ? w_mcursors_size * 2 : 16;
      multi_cursor *p = (multi_cursor *)realloc (w_mcursors, sizeof *p * size);
      if (!p)
        return 0;
      w_mcursors = p;
      w_mcursors_size = size;
    }

  memmove (w_mcursors + i + 1, w_mcursors + i,
           sizeof *w_mcursors * (w_nmcursors - i));
  w_mcursors[i].mc_point = point;
  w_mcursors[i].mc_mark = mark;
  w_nmcursors++;
  return 1;
}

void
Window::mc_remove_at (int i)
{
  memmove (w_mcursors + i, w_mcursors + i + 1,
           sizeof *w_mcursors * (w_nmcursors - i - 1));
  w_nmcursors--;
}

/* DIR (1: 下 -1: 上) の端から一行先へ足す。伸ばした向きと逆なら最後の一つを畳む */
int
Window::mc_extend (int dir)
{
  Buffer *bp = w_bufp;
  const int folded = bp->b_fold_columns != Buffer::FOLD_NONE;

  if (!w_nmcursors)
    {
      w_mc_column = (folded
                     ? bp->folded_point_column (w_point)
                     : bp->point_column (w_point));
      w_mc_direction = 0;
    }

  if (w_mc_direction && w_mc_direction != dir)
    {
      int i = w_mc_direction > 0 ? w_nmcursors - 1 : 0;
      bp->set_modified_region (w_mcursors[i].mc_point,
                               w_mcursors[i].mc_point + 1);
      mc_remove_at (i);
      if (!w_nmcursors)
        w_mc_direction = 0;
      return 1;
    }

  point_t base = w_point.p_point;
  if (w_nmcursors)
    base = (dir > 0
            ? max (base, w_mcursors[w_nmcursors - 1].mc_point)
            : min (base, w_mcursors[0].mc_point));

  /* goto_char は今の位置から辿るので、埋めてから渡す */
  Point point;
  point.p_point = 0;
  point.p_chunk = bp->b_chunkb;
  point.p_offset = 0;
  bp->goto_char (point, base);
  if (folded)
    {
      if (!bp->folded_forward_line (point, dir))
        return 0;
      bp->folded_forward_column (point, w_mc_column, 0, 0, 0);
      bp->check_range (point);
    }
  else
    {
      if (!bp->forward_line (point, dir))
        return 0;
      bp->forward_column (point, w_mc_column, 0, 0, 1);
      if (point.p_point < bp->b_contents.p1)
        bp->goto_char (point, bp->b_contents.p1);
    }

  if (point.p_point == w_point.p_point || !mc_add (point.p_point))
    return 0;

  w_mc_direction = dir;
  bp->set_modified_region (point.p_point, point.p_point + 1);
  return 1;
}

void
Window::mc_adjust_insertion (point_t opoint, int size)
{
  for (int i = w_nmcursors - 1; i >= 0; i--)
    {
      if (w_mcursors[i].mc_point <= opoint)
        break;
      w_mcursors[i].mc_point += size;
    }
  for (int i = 0; i < w_nmcursors; i++)
    if (w_mcursors[i].mc_mark > opoint)
      w_mcursors[i].mc_mark += size;
}

/* 同じ場所へ寄ったものを一つにまとめる */
void
Window::mc_merge ()
{
  int d = 0;
  for (int s = 1; s < w_nmcursors; s++)
    if (w_mcursors[s].mc_point != w_mcursors[d].mc_point)
      w_mcursors[++d] = w_mcursors[s];
  if (w_nmcursors)
    w_nmcursors = d + 1;
}

void
Window::mc_adjust_deletion (point_t from, int size)
{
  for (int i = w_nmcursors - 1; i >= 0; i--)
    {
      if (w_mcursors[i].mc_point <= from)
        break;
      w_mcursors[i].mc_point = max (point_t (w_mcursors[i].mc_point - size), from);
    }
  for (int i = 0; i < w_nmcursors; i++)
    if (w_mcursors[i].mc_mark > from)
      w_mcursors[i].mc_mark = max (point_t (w_mcursors[i].mc_mark - size), from);

  /* 一つずつ回している間は添字がずれると困るので、まとめるのは回し終えてから */
  if (w_mc_saved_point == NO_MARK_SET)
    mc_merge ();
}

/* 一つのカーソルで実行する間だけ w_point をそこへ移し、済んだら主カーソルへ戻す。
   例外で抜けても戻るよう、後始末をデストラクタに置く */
class mc_visit
{
  Window *v_wp;
public:
  mc_visit (Window *wp, point_t point) : v_wp (wp)
    {
      v_wp->w_mc_saved_point = v_wp->w_point.p_point;
      v_wp->w_bufp->goto_char (v_wp->w_point, point);
    }
  ~mc_visit ()
    {
      point_t saved = v_wp->w_mc_saved_point;
      v_wp->w_mc_saved_point = NO_MARK_SET;
      v_wp->w_bufp->goto_char (v_wp->w_point, saved);
    }
};

lisp
mc_command_execute (lisp command)
{
  Window *wp = selected_window ();
  if (!wp->w_nmcursors
      || memq (command, xsymbol_value (Vmulti_cursor_no_repeat_commands)))
    return Fcommand_execute (command, 0);

  Buffer *bp = wp->w_bufp;
  const long minibuf = app.minibuffer_enter_count;
  point_t p1 = wp->w_mcursors[0].mc_point;
  point_t p2 = wp->w_mcursors[wp->w_nmcursors - 1].mc_point + 1;

  lisp result = Fcommand_execute (command, 0);

  /* 主カーソルで一度実行して足場が動いたなら、残りは回さずに畳む */
  if (selected_window () != wp
      || wp->w_bufp != bp
      || app.minibuffer_enter_count != minibuf)
    {
      wp->mc_discard ();
      return result;
    }

  /* 後ろから回す。前のカーソルの位置は後ろでの編集に動かされない */
  for (int i = wp->w_nmcursors - 1; i >= 0; i--)
    {
      mc_visit visit (wp, wp->w_mcursors[i].mc_point);
      Fcommand_execute (command, 0);
      wp->w_mcursors[i].mc_point = wp->w_point.p_point;
    }

  wp->mc_merge ();
  int i = wp->mc_search (wp->w_point.p_point);
  if (i >= 0)
    wp->mc_remove_at (i);

  if (wp->w_nmcursors)
    {
      p1 = min (p1, wp->w_mcursors[0].mc_point);
      p2 = max (p2, point_t (wp->w_mcursors[wp->w_nmcursors - 1].mc_point + 1));
    }
  bp->set_modified_region (p1, p2);
  return result;
}

static lisp
mc_extend_lines (int dir, lisp n)
{
  Window *wp = selected_window ();
  long count = (!n || n == Qnil) ? 1 : fixnum_value (n);
  if (count < 0)
    {
      count = -count;
      dir = -dir;
    }
  long done = 0;
  while (done < count && wp->mc_extend (dir))
    done++;
  return done ? make_fixnum (done) : Qnil;
}

lisp
Fmulti_cursor_add_below (lisp n)
{
  return mc_extend_lines (1, n);
}

lisp
Fmulti_cursor_add_above (lisp n)
{
  return mc_extend_lines (-1, n);
}

lisp
Fmulti_cursor_clear ()
{
  Window *wp = selected_window ();
  if (!wp->w_nmcursors)
    return Qnil;
  wp->mc_discard ();
  return Qt;
}

lisp
Fmulti_cursor_list ()
{
  Window *wp = selected_window ();
  lisp r = Qnil;
  for (int i = wp->w_nmcursors - 1; i >= 0; i--)
    r = xcons (make_fixnum (wp->w_mcursors[i].mc_point), r);
  return r;
}
