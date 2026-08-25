#include <windows.h>
#include <malloc.h>

typedef unsigned long u_long;

static WCHAR *
skip_token (WCHAR *p)
{
  if (*p == '"')
    {
      for (p++; *p && *p != '"'; p++)
        ;
      if (*p == '"')
        p++;
    }
  else
    for (; *p && *p != ' ' && *p != '\t'; p++)
      ;
  return p;
}

static WCHAR *
skip_white (WCHAR *p)
{
  for (; *p == ' ' || *p == '\t'; p++)
    ;
  return p;
}

static WCHAR *
split (WCHAR *&beg)
{
  WCHAR *p = skip_token (beg);
  if (*beg == '"')
    {
      beg++;
      if (p[-1] == '"')
        p[-1] = 0;
    }
  else if (*p)
    *p++ = 0;
  return skip_white (p);
}

static WCHAR *
split (WCHAR *&beg, int &l)
{
  WCHAR *p = skip_token (beg);
  if (*beg == '"')
    {
      beg++;
      l = p - beg;
      if (p[-1] == '"')
        l--;
    }
  else
    l = p - beg;
  return skip_white (p);
}

static u_long
parse_long (const WCHAR *p)
{
  u_long val = 0;
  for (; *p >= '0' && *p <= '9'; p++)
    val = val * 10 + *p - '0';
  return val;
}

static inline int
char_upcase (int c)
{
  return c >= 'a' && c <= 'Z' ? c - ('a' - 'A') : c;
}

static int
bcasecmp (const WCHAR *p, const WCHAR *q, int size)
{
  const WCHAR *const pe = p + size;
  int f;
  for (f = 0; p < pe && !(f = char_upcase (*p) - char_upcase (*q)); p++, q++)
    ;
  return f;
}

/* 標準エラー出力は端末とやりとりするバイト列なので CP932 に落とす */
static void
doprint (const WCHAR *fmt, ...)
{
  WCHAR buf[1024];
  va_list ap;
  va_start (ap, fmt);
  wvsprintfW (buf, fmt, ap);
  va_end (ap);
  char b[sizeof buf * 2];
  int l = WideCharToMultiByte (CP_ACP, 0, buf, lstrlenW (buf),
                               b, sizeof b, 0, 0);
  DWORD n;
  WriteFile (GetStdHandle (STD_ERROR_HANDLE), b, l, &n, 0);
}

static void
syserror (int e, WCHAR *buf, int size)
{
  if (!FormatMessageW ((FORMAT_MESSAGE_FROM_SYSTEM
                        | FORMAT_MESSAGE_IGNORE_INSERTS
                        | FORMAT_MESSAGE_MAX_WIDTH_MASK),
                       0, e, GetUserDefaultLangID (),
                       buf, size, 0))
    wsprintfW (buf, L"error %d", e);
}

static int
cmdmatch (const WCHAR *p, const WCHAR *pe, const WCHAR *s)
{
  if (pe - p >= 4 && (!bcasecmp (pe - 4, L".exe", 4)
                      || !bcasecmp (pe - 4, L".com", 4)))
    pe -= 4;
  int l = lstrlenW (s);
  return pe - p >= l && !bcasecmp (pe - l, s, l);
}

static void
set_title (WCHAR *cmd)
{
  int cmdl;
  WCHAR *opt = split (cmd, cmdl);
  if (cmdmatch (cmd, cmd + cmdl, L"cmd")
      || cmdmatch (cmd, cmd + cmdl, L"command"))
    {
      int optl;
      WCHAR *arg = split (opt, optl);
      if (optl == 2 && !bcasecmp (opt, L"/c", 2))
        {
          cmd = arg;
          split (cmd, cmdl);
        }
    }

  WCHAR *title = (WCHAR *)_alloca ((cmdl + 1) * sizeof (WCHAR));
  memcpy (title, cmd, cmdl * sizeof (WCHAR));
  title[cmdl] = 0;
  SetConsoleTitleW (title);
}

int
main (void)
{
  WCHAR buf[256];
  /* 区切るときに書き込むので、コマンドラインは写してから使う */
  const WCHAR *const cl = GetCommandLineW ();
  WCHAR *const clcopy = (WCHAR *)_alloca ((lstrlenW (cl) + 1) * sizeof (WCHAR));
  lstrcpyW (clcopy, cl);

  WCHAR *myname = skip_white (clcopy);
  WCHAR *opt = split (myname);
  WORD show = 0;
  WCHAR *event;
  if (!wcsncmp (opt, L"-s", 2))
    {
      if (lstrlenW (opt) > 2)
        show = static_cast <WORD> (parse_long (opt + 2));
      event = split (opt);
    }
  else
    {
      event = opt;
    }
  WCHAR *cmdline = split (event);
  WCHAR *dir = 0;
  int no_events = !lstrcmpW (event, L"--");

  if (no_events)
    {
      dir = cmdline;
      cmdline = split (dir);
    }

  set_title (cmdline);

  PROCESS_INFORMATION pi;
  STARTUPINFOW si = {sizeof si};

  si.dwFlags = STARTF_USESTDHANDLES;
  if (show)
    {
      si.dwFlags |= STARTF_USESHOWWINDOW;
      si.wShowWindow = show;
    }
  si.hStdInput = GetStdHandle (STD_INPUT_HANDLE);
  si.hStdOutput = GetStdHandle (STD_OUTPUT_HANDLE);
  si.hStdError = GetStdHandle (STD_ERROR_HANDLE);

  if (!CreateProcessW (0, cmdline, 0, 0, 1, CREATE_NEW_PROCESS_GROUP,
                       0, dir, &si, &pi))
    {
      syserror (GetLastError (), buf, sizeof buf / sizeof *buf);
      doprint (L"%s: %s: %s\n", myname, cmdline, buf);
      ExitProcess (2);
    }

  CloseHandle (pi.hThread);

  if (no_events)
    {
      if (WaitForSingleObject (pi.hProcess, INFINITE) == WAIT_FAILED)
        {
          syserror (GetLastError (), buf, sizeof buf / sizeof *buf);
          doprint (L"%s: %s\n", myname, buf);
          ExitProcess (2);
        }
    }
  else
    {
      HANDLE hevent = HANDLE (parse_long (event));

      HANDLE objects[2];
      objects[0] = hevent;
      objects[1] = pi.hProcess;
      while (1)
        {
          DWORD r = WaitForMultipleObjects (2, objects, 0, INFINITE);
          if (r == WAIT_FAILED)
            {
              syserror (GetLastError (), buf, sizeof buf / sizeof *buf);
              doprint (L"%s: %s\n", myname, buf);
              ExitProcess (2);
            }
          if (r == WAIT_OBJECT_0 + 1)
            break;

          GenerateConsoleCtrlEvent (CTRL_BREAK_EVENT, pi.dwProcessId);
          if (WaitForSingleObject (pi.hProcess, 3000) == WAIT_TIMEOUT)
            GenerateConsoleCtrlEvent (CTRL_C_EVENT, pi.dwProcessId);
          ResetEvent (hevent);
        }
    }

  DWORD code;
  GetExitCodeProcess (pi.hProcess, &code);
  ExitProcess (code);
}
