// -*-C++-*-
#ifndef _message_h_
# define _message_h_

inline lisp
make_message (int n)
{
  return make_immediate (Lmessage, n);
}

inline int
messagep (lisp x)
{
  return immediate_tag (x) == Lmessage;
}

inline int
xmessage_code (lisp x)
{
  assert (messagep (x));
  return ximmediate_data (x);
}

#endif
