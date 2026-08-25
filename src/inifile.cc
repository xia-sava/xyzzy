#include "inifile.h"

static const WCHAR *
skip_white (const WCHAR *p)
{
  while (*p == ' ' || *p == '\t')
    p++;
  return p;
}

static const WCHAR *
trim_right (const WCHAR *p, const WCHAR *e)
{
  while (e > p && (e[-1] == ' ' || e[-1] == '\t'))
    e--;
  return e;
}

/* 節の見出しなら、その名前を取り出す */
static int
section_line_p (const std::wstring &line, std::wstring &name)
{
  const WCHAR *p = skip_white (line.c_str ());
  if (*p != '[')
    return 0;
  const WCHAR *e = wcschr (++p, ']');
  if (!e)
    return 0;
  name.assign (p, e - p);
  return 1;
}

/* 値を持つ行なら、そのキーの名前を取り出す */
static int
name_line_p (const std::wstring &line, std::wstring &name)
{
  const WCHAR *p = skip_white (line.c_str ());
  if (!*p || *p == ';' || *p == '#' || *p == '[')
    return 0;
  const WCHAR *e = wcschr (p, '=');
  if (!e)
    return 0;
  name.assign (p, trim_right (p, e) - p);
  return 1;
}

/* 前後の空白と、対になった引用符をひとつ外す */
static std::wstring
line_value (const std::wstring &line)
{
  const WCHAR *p = wcschr (line.c_str (), '=');
  if (!p)
    return std::wstring ();
  p = skip_white (p + 1);
  const WCHAR *e = trim_right (p, p + wcslen (p));
  if (e - p >= 2 && *p == '"' && e[-1] == '"')
    {
      p++;
      e--;
    }
  return std::wstring (p, e - p);
}

static void
decode (const std::string &b, std::wstring &text)
{
  const char *p = b.data ();
  int l = int (b.size ());

  if (l >= 2 && (unsigned char)p[0] == 0xff && (unsigned char)p[1] == 0xfe)
    {
      text.assign ((const WCHAR *)(p + 2), (l - 2) / 2);
      return;
    }

  int cp = CP_UTF8;
  if (l >= 3 && (unsigned char)p[0] == 0xef && (unsigned char)p[1] == 0xbb
      && (unsigned char)p[2] == 0xbf)
    {
      p += 3;
      l -= 3;
    }
  // UTF-8 として読めない並びなら、古い版が書いた ANSI とみなす
  else if (!MultiByteToWideChar (CP_UTF8, MB_ERR_INVALID_CHARS, p, l, 0, 0))
    cp = CP_ACP;

  int n = MultiByteToWideChar (cp, 0, p, l, 0, 0);
  text.resize (n);
  if (n)
    MultiByteToWideChar (cp, 0, p, l, &text[0], n);
}

void
IniFile::open (const WCHAR *path)
{
  i_path = path;
  i_lines.clear ();
  i_loaded = 1;
  i_modified = 0;

  HANDLE h = CreateFileW (path, GENERIC_READ, FILE_SHARE_READ, 0,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (h == INVALID_HANDLE_VALUE)
    return;

  std::string b;
  DWORD size = GetFileSize (h, 0);
  if (size && size != INVALID_FILE_SIZE)
    {
      b.resize (size);
      DWORD n = 0;
      if (!ReadFile (h, &b[0], size, &n, 0))
        n = 0;
      b.resize (n);
    }
  CloseHandle (h);
  if (b.empty ())
    return;

  std::wstring text;
  decode (b, text);

  for (size_t p = 0;;)
    {
      size_t nl = text.find (L'\n', p);
      size_t e = nl == std::wstring::npos ? text.size () : nl;
      size_t t = e;
      if (t > p && text[t - 1] == L'\r')
        t--;
      i_lines.push_back (text.substr (p, t - p));
      if (nl == std::wstring::npos)
        break;
      p = nl + 1;
    }
  if (!i_lines.empty () && i_lines.back ().empty ())
    i_lines.pop_back ();
}

int
IniFile::find_section (const WCHAR *section) const
{
  std::wstring name;
  for (int i = 0; i < int (i_lines.size ()); i++)
    if (section_line_p (i_lines[i], name) && !_wcsicmp (name.c_str (), section))
      return i;
  return -1;
}

int
IniFile::section_end (int s) const
{
  std::wstring name;
  int i;
  for (i = s + 1; i < int (i_lines.size ()); i++)
    if (section_line_p (i_lines[i], name))
      break;
  // 節の終わりに続く空行は、次の節のものとして残す
  while (i > s + 1 && i_lines[i - 1].empty ())
    i--;
  return i;
}

int
IniFile::find_name (int from, int to, const WCHAR *name) const
{
  std::wstring key;
  for (int i = from; i < to; i++)
    if (name_line_p (i_lines[i], key) && !_wcsicmp (key.c_str (), name))
      return i;
  return -1;
}

int
IniFile::get (const WCHAR *section, const WCHAR *name, WCHAR *buf, int size) const
{
  if (size <= 0)
    return 0;
  *buf = 0;

  int s = find_section (section);
  if (s < 0)
    {
      if (!name && size >= 2)
        buf[1] = 0;
      return 0;
    }
  int e = section_end (s);

  if (!name)
    {
      WCHAR *b = buf, *be = buf + size - 1;
      std::wstring key;
      for (int i = s + 1; i < e; i++)
        if (name_line_p (i_lines[i], key))
          {
            int l = int (key.size ());
            if (b + l + 1 > be)
              break;
            memcpy (b, key.c_str (), l * sizeof (WCHAR));
            b += l;
            *b++ = 0;
          }
      *b = 0;
      return int (b - buf);
    }

  int i = find_name (s + 1, e, name);
  if (i < 0)
    return 0;
  std::wstring value = line_value (i_lines[i]);
  int l = int (value.size ());
  if (l > size - 1)
    l = size - 1;
  memcpy (buf, value.c_str (), l * sizeof (WCHAR));
  buf[l] = 0;
  return l;
}

int
IniFile::get_int (const WCHAR *section, const WCHAR *name, int defalt) const
{
  WCHAR buf[64];
  if (!get (section, name, buf, 64))
    return defalt;
  WCHAR *e;
  long v = wcstol (buf, &e, 10);
  return e == buf ? defalt : int (v);
}

void
IniFile::set (const WCHAR *section, const WCHAR *name, const WCHAR *value)
{
  int s = find_section (section);
  if (s < 0)
    {
      if (!i_lines.empty () && !i_lines.back ().empty ())
        i_lines.push_back (std::wstring ());
      std::wstring head (L"[");
      head += section;
      head += L']';
      i_lines.push_back (head);
      s = int (i_lines.size ()) - 1;
    }

  std::wstring line (name);
  line += L'=';
  line += value;

  int e = section_end (s);
  int i = find_name (s + 1, e, name);
  if (i >= 0)
    {
      if (i_lines[i] == line)
        return;
      i_lines[i] = line;
    }
  else
    i_lines.insert (i_lines.begin () + e, line);
  i_modified = 1;
}

void
IniFile::erase (const WCHAR *section)
{
  int s = find_section (section);
  if (s < 0)
    return;
  i_lines.erase (i_lines.begin () + s, i_lines.begin () + section_end (s));
  i_modified = 1;
}

int
IniFile::flush ()
{
  if (!i_modified)
    return 1;

  std::wstring text;
  for (size_t i = 0; i < i_lines.size (); i++)
    {
      text += i_lines[i];
      text += L"\r\n";
    }

  std::string b;
  int l = WideCharToMultiByte (CP_UTF8, 0, text.c_str (), int (text.size ()),
                               0, 0, 0, 0);
  if (l)
    {
      b.resize (l);
      WideCharToMultiByte (CP_UTF8, 0, text.c_str (), int (text.size ()),
                           &b[0], l, 0, 0);
    }

  // 書き終えてから置き換える。途中で落ちても元の設定が残る
  std::wstring tmp (i_path);
  tmp += L".tmp";
  HANDLE h = CreateFileW (tmp.c_str (), GENERIC_WRITE, 0, 0,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  DWORD n = 0;
  int ok = !l || (WriteFile (h, &b[0], l, &n, 0) && int (n) == l);
  CloseHandle (h);
  if (!ok || !MoveFileExW (tmp.c_str (), i_path.c_str (), MOVEFILE_REPLACE_EXISTING))
    {
      DeleteFileW (tmp.c_str ());
      return 0;
    }
  i_modified = 0;
  return 1;
}
