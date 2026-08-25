#ifndef _binfo_h_
#define _binfo_h_

#include "version.h"

class buffer_info
{
  const Window *const b_wp;
  const Buffer *const b_bufp;
  WCHAR **const b_posp;
  WCHAR **const b_percentp;
  int *const b_ime;
  static const WCHAR *const b_eol_name[];

  WCHAR *minor_mode (lisp, WCHAR *, WCHAR *, int &) const;
public:
  buffer_info (const Window *wp, const Buffer *bp, WCHAR **posp, int *ime, WCHAR **percentp)
       : b_wp (wp), b_bufp (bp), b_posp (posp), b_ime (ime), b_percentp(percentp) {}
  WCHAR *format (lisp, WCHAR *, WCHAR *) const;
  WCHAR *modified (WCHAR *, int) const;
  WCHAR *read_only (WCHAR *, int) const;
  WCHAR *progname (WCHAR *b, WCHAR *be) const
    {return stpncpy (b, ProgramName, be - b);}
  WCHAR *version (WCHAR *, WCHAR *, int) const;
  WCHAR *buffer_name (WCHAR *, WCHAR *) const;
  WCHAR *file_name (WCHAR *, WCHAR *, int) const;
  WCHAR *file_or_buffer_name (WCHAR *, WCHAR *, int) const;
  WCHAR *mode_name (WCHAR *, WCHAR *, int) const;
  WCHAR *encoding (WCHAR *b, WCHAR *be) const
    {return w2u (b, be, xchar_encoding_name (b_bufp->lchar_encoding));}
  WCHAR *encoding_lang (WCHAR *, WCHAR *) const;
  WCHAR *eol_code (WCHAR *b, WCHAR *be) const
    {return stpncpy (b, b_eol_name[b_bufp->b_eol_code], be - b);}
  WCHAR *ime_mode (WCHAR *, WCHAR *) const;
  WCHAR *position (WCHAR *, WCHAR *) const;
  WCHAR *host_name (WCHAR *, WCHAR *, int) const;
  WCHAR *process_id (WCHAR *, WCHAR *) const;
  WCHAR *admin_user (WCHAR *, WCHAR *) const;
  WCHAR *percent(WCHAR *, WCHAR *) const;
};

#endif
