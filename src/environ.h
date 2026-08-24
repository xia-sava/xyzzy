#ifndef _environ_h_
# define _environ_h_

class WindowGeometry
{
public:
  static int save_window_size;
  static int save_window_snap_size;
  static int save_window_position;
  static int restore_window_size;
  static int restore_window_position;
  static int load_geometry (int, POINT *, SIZE *);
  static void save_geometry ();
};

class Registry
{
protected:
  static const WCHAR base[];
  HKEY hkey;
  Registry ();
  ~Registry ();
public:
  static const WCHAR Settings[];
  int fail () const;
};

inline
Registry::Registry ()
     : hkey (0)
{
}

inline
Registry::~Registry ()
{
  if (hkey)
    RegCloseKey (hkey);
}

inline int
Registry::fail () const
{
  return !hkey;
}

class ReadRegistry: public Registry
{
protected:
  void open_local (const WCHAR *);
public:
  int get (const WCHAR *, void *, DWORD, DWORD) const;
  int get (const WCHAR *, int *) const;
  int get (const WCHAR *, long *) const;
  int get (const WCHAR *, WCHAR *, int) const;
  int get (const WCHAR *, void *, int) const;
  int query (const WCHAR *, DWORD *) const;
  ReadRegistry (const WCHAR *);
  ReadRegistry (HKEY, const WCHAR *);
};

inline
ReadRegistry::ReadRegistry (const WCHAR *subkey)
{
  open_local (subkey);
}

inline int
ReadRegistry::get (const WCHAR *key, int *x) const
{
  return get (key, x, sizeof *x, REG_DWORD) == sizeof *x;
}

inline int
ReadRegistry::get (const WCHAR *key, long *x) const
{
  return get (key, x, sizeof *x, REG_DWORD) == sizeof *x;
}

inline int
ReadRegistry::get (const WCHAR *key, WCHAR *buf, int size) const
{
  return get (key, buf, size * sizeof (WCHAR), REG_SZ);
}

inline int
ReadRegistry::get (const WCHAR *key, void *buf, int size) const
{
  return get (key, buf, size, REG_BINARY);
}

class WriteRegistry: public Registry
{
public:
  int set (const WCHAR *, DWORD, const void *, int) const;
  int set (const WCHAR *, long) const;
  int set (const WCHAR *, const WCHAR *) const;
  int set (const WCHAR *, const WCHAR *, int) const;
  int set (const WCHAR *, const void *, int) const;
  int remove (const WCHAR *) const;
  WriteRegistry (const WCHAR *);
};

inline int
WriteRegistry::set (const WCHAR *key, long val) const
{
  return set (key, REG_DWORD, &val, sizeof val);
}

inline int
WriteRegistry::set (const WCHAR *key, const WCHAR *val) const
{
  return set (key, REG_SZ, val, (wcslen (val) + 1) * sizeof (WCHAR));
}

inline int
WriteRegistry::set (const WCHAR *key, const WCHAR *val, int size) const
{
  return set (key, REG_SZ, val, size * sizeof (WCHAR));
}

inline int
WriteRegistry::set (const WCHAR *key, const void *val, int size) const
{
  return set (key, REG_BINARY, val, size);
}

class EnumRegistry: public ReadRegistry
{
public:
  EnumRegistry (const WCHAR *subkey) : ReadRegistry (subkey) {}
  EnumRegistry (HKEY h, const WCHAR *subkey) : ReadRegistry (h, subkey) {}
  operator HKEY () const {return hkey;}
};

struct decoded_time
{
  int year;
  int mon;
  int day;
  int hour;
  int min;
  int sec;
  int dow;
  int timezone;
  int daylight;
};

void decode_universal_time (lisp, decoded_time *);
lisp decoded_time_to_universal_time (int, int, int, int, int, int, int);
lisp file_time_to_universal_time (const FILETIME &);

#endif
