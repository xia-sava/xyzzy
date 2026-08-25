#ifndef _inifile_h_
#define _inifile_h_

#include <windows.h>
#include <string>
#include <vector>

/* 節とキーで引く設定ファイル。行をそのまま持つので、コメントも並び順も、
   こちらが知らないキーもそのまま残る。ファイルは UTF-8 で書く */
class IniFile
{
  std::wstring i_path;
  std::vector <std::wstring> i_lines;
  int i_loaded;
  int i_modified;

  int find_section (const WCHAR *) const;
  int section_end (int) const;
  int find_name (int, int, const WCHAR *) const;

public:
  IniFile () : i_loaded (0), i_modified (0) {}

  int loaded () const {return i_loaded;}

  void open (const WCHAR *path);

  /* 値を buf へ写して、その文字数を返す。名前を省くと、その節のキーの名前を
     NUL 区切りで並べ、終わりを NUL 二つで示す */
  int get (const WCHAR *section, const WCHAR *name, WCHAR *buf, int size) const;
  int get_int (const WCHAR *section, const WCHAR *name, int defalt) const;

  void set (const WCHAR *section, const WCHAR *name, const WCHAR *value);
  void erase (const WCHAR *section);

  int flush ();
};

#endif /* _inifile_h_ */
