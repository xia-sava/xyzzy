#include "stdafx.h"
#include "ed.h"

/* カーソルの選択範囲。窓の側と同じく、marker と今の位置から決まる */
static int
mc_selection_span (const multi_cursor &c, point_t &p1, point_t &p2)
{
  if ((c.mc_selection_type & Buffer::SELECTION_TYPE_MASK) == Buffer::SELECTION_VOID
      || c.mc_selection_marker == NO_MARK_SET)
    return 0;
  p1 = min (c.mc_point, c.mc_selection_marker);
  p2 = max (c.mc_point, c.mc_selection_marker);
  return p1 != p2;
}

/* カーソルが画面で占める範囲。選択していればそこまで含む */
static void
mc_span (const multi_cursor &c, point_t &p1, point_t &p2)
{
  p1 = c.mc_point;
  p2 = c.mc_point + 1;
  point_t s1, s2;
  if (mc_selection_span (c, s1, s2))
    {
      p1 = min (p1, s1);
      p2 = max (p2, s2);
    }
}

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
  point_t p1, p2, t1, t2;
  mc_span (w_mcursors[0], p1, t2);
  mc_span (w_mcursors[w_nmcursors - 1], t1, p2);
  w_bufp->set_modified_region (p1, p2);
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

/* POINT がどれかのカーソルの選択の内側か */
int
Window::mc_selection_p (point_t point) const
{
  for (int i = 0; i < w_nmcursors; i++)
    {
      point_t p1, p2;
      if (mc_selection_span (w_mcursors[i], p1, p2)
          && point >= p1 && point < p2)
        return 1;
    }
  return 0;
}

/* 一時の選択は、伸ばす命令が続かなければ次の描画で解ける。窓の側と同じことを
   カーソルの分にもする。解けて描き直しの要る範囲を返す (無ければ p1 が -1) */
Region
Window::mc_step_pre_selection ()
{
  Region r;
  r.p1 = -1;
  r.p2 = -1;
  for (int i = 0; i < w_nmcursors; i++)
    {
      multi_cursor &c = w_mcursors[i];
      if ((c.mc_selection_type & (Buffer::CONTINUE_PRE_SELECTION
                                  | Buffer::PRE_SELECTION))
          == Buffer::PRE_SELECTION)
        {
          point_t p1, p2;
          if (mc_selection_span (c, p1, p2))
            {
              if (r.p1 == -1)
                {
                  r.p1 = p1;
                  r.p2 = p2;
                }
              else
                {
                  r.p1 = min (r.p1, p1);
                  r.p2 = max (r.p2, p2);
                }
            }
          c.mc_selection_type = Buffer::SELECTION_VOID;
          c.mc_selection_point = NO_MARK_SET;
          c.mc_selection_marker = NO_MARK_SET;
        }
      (int &)c.mc_selection_type &= ~Buffer::CONTINUE_PRE_SELECTION;
    }
  return r;
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
  multi_cursor &c = w_mcursors[i];
  c.mc_point = point;
  c.mc_mark = mark;
  c.mc_selection_type = Buffer::SELECTION_VOID;
  c.mc_selection_point = NO_MARK_SET;
  c.mc_selection_marker = NO_MARK_SET;
  c.mc_selection_column = 0;
  c.mc_locals = Qnil;
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

/* 重なったものを一つにまとめる。選択が隣に届いたときも畳む */
void
Window::mc_merge ()
{
  if (!w_nmcursors)
    return;
  int d = 0;
  point_t dp1, dp2;
  mc_span (w_mcursors[0], dp1, dp2);
  for (int s = 1; s < w_nmcursors; s++)
    {
      point_t sp1, sp2;
      mc_span (w_mcursors[s], sp1, sp2);
      if (w_mcursors[s].mc_point == w_mcursors[d].mc_point || sp1 < dp2)
        {
          dp2 = max (dp2, sp2);
          continue;
        }
      w_mcursors[++d] = w_mcursors[s];
      dp1 = sp1;
      dp2 = sp2;
    }
  w_nmcursors = d + 1;
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
      point_t p1, p2;
      mc_span (w_mcursors[i], p1, p2);
      bp->set_modified_region (p1, p2);
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

static void
mc_adjust_one_insertion (multi_cursor &c, point_t opoint, int size)
{
#define ADJ(P) if ((P) > opoint) (P) += size
  ADJ (c.mc_point);
  ADJ (c.mc_mark);
  ADJ (c.mc_selection_point);
  ADJ (c.mc_selection_marker);
#undef ADJ
}

static void
mc_adjust_one_deletion (multi_cursor &c, point_t from, int size)
{
#define ADJ(P) if ((P) > from) (P) = max (point_t ((P) - size), from)
  ADJ (c.mc_point);
  ADJ (c.mc_mark);
  ADJ (c.mc_selection_point);
  ADJ (c.mc_selection_marker);
#undef ADJ
}

void
Window::mc_adjust_insertion (point_t opoint, int size)
{
  for (int i = 0; i < w_nmcursors; i++)
    mc_adjust_one_insertion (w_mcursors[i], opoint, size);
  if (w_mc_visiting)
    mc_adjust_one_insertion (w_mc_saved, opoint, size);
}

void
Window::mc_adjust_deletion (point_t from, int size)
{
  for (int i = 0; i < w_nmcursors; i++)
    mc_adjust_one_deletion (w_mcursors[i], from, size);
  if (w_mc_visiting)
    mc_adjust_one_deletion (w_mc_saved, from, size);

  /* 一つずつ回している間は添字がずれると困るので、まとめるのは回し終えてから */
  if (!w_mc_visiting)
    mc_merge ();
}

/* 窓が持つカーソルの一式を、カーソルの形へ写す。逆も同じ並びで */
static void
mc_from_window (multi_cursor &c, const Window *wp)
{
  c.mc_point = wp->w_point.p_point;
  c.mc_mark = wp->w_mark;
  c.mc_selection_type = wp->w_selection_type;
  c.mc_selection_point = wp->w_selection_point;
  c.mc_selection_marker = wp->w_selection_marker;
  c.mc_selection_column = wp->w_selection_column;
}

static void
mc_to_window (Window *wp, const multi_cursor &c)
{
  wp->w_mark = c.mc_mark;
  wp->w_selection_type = c.mc_selection_type;
  wp->w_selection_point = c.mc_selection_point;
  wp->w_selection_marker = c.mc_selection_marker;
  wp->w_selection_column = c.mc_selection_column;
  wp->w_bufp->goto_char (wp->w_point, c.mc_point);
}

/* 一つのカーソルで実行する間だけ窓の側をそのカーソルのものにし、済んだら
   結果を書き戻して主カーソルへ返す。例外で抜けても返るようデストラクタに置く */
class mc_visit
{
  Window *v_wp;
  int v_index;
public:
  mc_visit (Window *wp, int i) : v_wp (wp), v_index (i)
    {
      mc_from_window (wp->w_mc_saved, wp);
      wp->w_mc_visiting = 1;
      mc_to_window (wp, wp->w_mcursors[i]);
    }
  ~mc_visit ()
    {
      multi_cursor locals = v_wp->w_mcursors[v_index];
      mc_from_window (v_wp->w_mcursors[v_index], v_wp);
      v_wp->w_mcursors[v_index].mc_locals = locals.mc_locals;
      v_wp->w_mc_visiting = 0;
      mc_to_window (v_wp, v_wp->w_mc_saved);
    }
};

/* カーソルが抱えている値を変数へ移す。まだ持っていなければ今の値のまま */
static void
mc_load_locals (lisp vals)
{
  lisp vars = xsymbol_value (Vmulti_cursor_local_variables);
  for (; consp (vars) && consp (vals); vars = xcdr (vars), vals = xcdr (vals))
    {
      lisp sym = xcar (vars);
      if (symbolp (sym))
        xsymbol_value (sym) = xcar (vals);
    }
}

/* 今の変数の値を集める。OLD の長さが合えば中身を差し替えて使い回す */
static lisp
mc_store_locals (lisp old)
{
  lisp vars = xsymbol_value (Vmulti_cursor_local_variables);

  lisp v = vars, p = old;
  while (consp (v) && consp (p))
    {
      v = xcdr (v);
      p = xcdr (p);
    }
  if (!consp (v) && !consp (p))
    {
      for (v = vars, p = old; consp (v); v = xcdr (v), p = xcdr (p))
        {
          lisp sym = xcar (v);
          if (symbolp (sym))
            xcar (p) = xsymbol_value (sym);
        }
      return old;
    }

  lisp r = Qnil;
  protect_gc gcpro (r);
  for (v = vars; consp (v); v = xcdr (v))
    {
      lisp sym = xcar (v);
      r = xcons (symbolp (sym) ? xsymbol_value (sym) : Qnil, r);
    }
  return Fnreverse (r);
}

/* 回している間に触った変数を、済んだら主カーソルのものへ戻す */
class mc_locals_guard
{
  lisp v_saved;
  protect_gc v_gcpro;
public:
  mc_locals_guard () : v_saved (mc_store_locals (Qnil)), v_gcpro (v_saved) {}
  ~mc_locals_guard () {mc_load_locals (v_saved);}
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
  point_t p1, p2, t1, t2;
  mc_span (wp->w_mcursors[0], p1, t2);
  mc_span (wp->w_mcursors[wp->w_nmcursors - 1], t1, p2);

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
  {
    mc_locals_guard locals;
    for (int i = wp->w_nmcursors - 1; i >= 0; i--)
      {
        mc_visit visit (wp, i);
        mc_load_locals (wp->w_mcursors[i].mc_locals);
        Fcommand_execute (command, 0);
        wp->w_mcursors[i].mc_locals = mc_store_locals (wp->w_mcursors[i].mc_locals);
      }
  }

  wp->mc_merge ();
  int i = wp->mc_search (wp->w_point.p_point);
  if (i >= 0)
    wp->mc_remove_at (i);

  if (wp->w_nmcursors)
    {
      point_t q1, q2;
      mc_span (wp->w_mcursors[0], q1, t2);
      mc_span (wp->w_mcursors[wp->w_nmcursors - 1], t1, q2);
      p1 = min (p1, q1);
      p2 = max (p2, q2);
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
