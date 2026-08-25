#include "stdafx.h"
#include "ed.h"
#include "dyn-handle.h"
#include "vwin32.h"

class NetPassDlg
{
  HWND hwnd;
public:
  char username[256];
  char passwd[256];
  const char *remote;

private:
  static BOOL CALLBACK netpass_dlgproc (HWND, UINT, WPARAM, LPARAM);
  BOOL dlgproc (UINT, WPARAM, LPARAM);
  void do_command (int, int);
  void init_dialog ();

public:
  NetPassDlg (const char *);
  int do_modal ();
};

NetPassDlg::NetPassDlg (const char *r)
     : remote (r)
{
  *username = 0;
  *passwd = 0;
}

void
NetPassDlg::do_command (int id, int code)
{
  switch (id)
    {
    case IDOK:
      GetDlgItemText (hwnd, IDC_USERNAME, username, sizeof username);
      GetDlgItemText (hwnd, IDC_PASSWD, passwd, sizeof passwd);
      /* fall thru... */
    case IDCANCEL:
      EndDialog (hwnd, id);
      break;
    }
}

void
NetPassDlg::init_dialog ()
{
  center_window (hwnd);
  set_window_icon (hwnd);
  SetDlgItemText (hwnd, IDC_SHARE_NAME, remote);
}

BOOL
NetPassDlg::dlgproc (UINT msg, WPARAM wparam, LPARAM lparam)
{
  switch (msg)
    {
    case WM_INITDIALOG:
      init_dialog ();
      return 1;

    case WM_COMMAND:
      do_command (LOWORD (wparam), HIWORD (wparam));
      return 1;

    default:
      return 0;
    }
}

BOOL CALLBACK
NetPassDlg::netpass_dlgproc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  NetPassDlg *p;
  if (msg == WM_INITDIALOG)
    {
      p = (NetPassDlg *)lparam;
      SetWindowLong (hwnd, DWL_USER, lparam);
      p->hwnd = hwnd;
    }
  else
    {
      p = (NetPassDlg *)GetWindowLong (hwnd, DWL_USER);
      if (!p)
        return 0;
    }
  return p->dlgproc (msg, wparam, lparam);
}

int
NetPassDlg::do_modal ()
{
  return DialogBoxParam (app.hinst, MAKEINTRESOURCE (IDD_NETPASSWD),
                         get_active_window (), netpass_dlgproc, LPARAM (this)) == IDOK;
}

#define WINFS_CALL1(TYPE, FAILED, PATH, FN) \
  WINFS_CALL (TYPE, FAILED, askpass (PATH), FN)
#define WINFS_CALL2(TYPE, FAILED, PATH1, PATH2, FN) \
  WINFS_CALL (TYPE, FAILED, askpass (PATH1, PATH2), FN)
#define WINFS_CALL(TYPE, FAILED, ASKPASS, FN) \
  { TYPE r = ::FN; \
    if (r == (FAILED) && ASKPASS) \
      r = ::FN; \
    return r; }

#define WINFS_MAPSL(PATH) \
  { char *__path = (char *)alloca (strlen (PATH) + 1); \
    strcpy (__path, (PATH)); \
    map_sl_to_backsl (__path); \
    (PATH) = __path; }

static const char *
skip_share (const char *path, int noshare_ok)
{
  const char *p = path;
  if ((*p != '/' && *p != '\\')
      || (p[1] != '/' && p[1] != '\\'))
    return 0;
  p = find_slash (p + 2);
  if (p)
    {
      const char *e = find_slash (p + 1);
      return e ? e : p + strlen (p);
    }
  return noshare_ok ? path + strlen (path) : 0;
}

static int
try_connect (char *remote, int e)
{
  NETRESOURCE nr;
  nr.dwType = RESOURCETYPE_DISK;
  nr.lpLocalName = 0;
  nr.lpRemoteName = remote;
  nr.lpProvider = 0;

  if (e == ERROR_ACCESS_DENIED
      && WNetAddConnection2 (&nr, 0, 0, 0) == NO_ERROR)
    return 1;

  while (1)
    {
      NetPassDlg d (remote);
      if (!d.do_modal ())
        return 0;

      switch (WNetAddConnection2 (&nr, d.passwd, d.username, 0))
        {
        case NO_ERROR:
          return 1;

        case ERROR_INVALID_PASSWORD:
        case ERROR_LOGON_FAILURE:
        case ERROR_ACCESS_DENIED:
          break;

        default:
          return 0;
        }
    }
}

static int
askpass1 (const char *path, int noshare_ok)
{
  if (!path)
    return 0;

  int e = GetLastError ();
  switch (e)
    {
    default:
      return 0;

    case ERROR_ACCESS_DENIED:
    case ERROR_INVALID_PASSWORD:
    case ERROR_LOGON_FAILURE:
      break;
    }

  const char *root = skip_share (path, noshare_ok);
  if (!root)
    return 0;
  int l = root - path;
  char *remote = (char *)alloca (l + 1);
  memcpy (remote, path, l);
  remote[l] = 0;
  map_sl_to_backsl (remote);
  if (!_stricmp (WINFS::wfs_share_cache, remote))
    return 0;
  if (try_connect (remote, e))
    {
      *WINFS::wfs_share_cache = 0;
      return 1;
    }
  strcpy (WINFS::wfs_share_cache, remote);
  SetLastError (e);
  return 0;
}

static inline int
askpass (const char *path)
{
  return askpass1 (path, 0);
}

static inline int
askpass_noshare (const char *path)
{
  return askpass1 (path, 1);
}

static inline int
askpass (const char *path1, const char *path2)
{
  return askpass1 (path1, 0) || askpass1 (path2, 0);
}

/* パスは UTF-8 のバイト列のまま持ち回る。API へ渡す手前で UTF-16 にする */
class wpath
{
  WCHAR w_buf[PATH_MAX + 1];
  int w_null;
public:
  wpath (LPCSTR path) : w_null (!path)
    {
      *w_buf = 0;
      if (path && strlen (path) <= PATH_MAX)
        u82u (w_buf, path);
    }
  operator LPCWSTR () const {return w_null ? 0 : w_buf;}
};

/* API から返ったパスを、持ち回る形の文字列にする */
static DWORD
store_path (LPSTR buf, DWORD size, LPCWSTR w)
{
  size_t l = u2u8l (w);
  if (l >= size)
    return 0;
  u2u8 (buf, w);
  return DWORD (l);
}

char WINFS::wfs_share_cache[MAX_PATH * 2];

const WINFS::GETDISKFREESPACEEX WINFS::GetDiskFreeSpaceEx =
  (WINFS::GETDISKFREESPACEEX)GetProcAddress (GetModuleHandleW (L"KERNEL32"),
                                             "GetDiskFreeSpaceExW");

BOOL WINAPI
WINFS::CreateDirectory (LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
  wpath w (lpPathName);
  WINFS_CALL1 (BOOL, FALSE, lpPathName, CreateDirectoryW (w, lpSecurityAttributes));
}

HANDLE WINAPI
WINFS::CreateFile (LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
  wpath w (lpFileName);
  HANDLE r = ::CreateFileW (w, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                            dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
  if (r != INVALID_HANDLE_VALUE)
    return r;
  if (!sysdep.WinNTp () || !(dwFlagsAndAttributes & FILE_FLAG_BACKUP_SEMANTICS))
    {
      int e = GetLastError ();
      if (e == ERROR_ACCESS_DENIED)
        {
          DWORD a = ::GetFileAttributesW (w);
          SetLastError (e);
          if (a != -1 && a & FILE_ATTRIBUTE_DIRECTORY)
            return r;
        }
    }
  if (askpass (lpFileName))
    r = ::CreateFileW (w, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                       dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
  return r;
}

BOOL WINAPI
WINFS::DeleteFile (LPCSTR lpFileName)
{
  wpath w (lpFileName);
  WINFS_CALL1 (BOOL, FALSE, lpFileName, DeleteFileW (w));
}

static void
store_find_data (LPWIN32_FIND_DATAA a, const WIN32_FIND_DATAW &w)
{
  a->dwFileAttributes = w.dwFileAttributes;
  a->ftCreationTime = w.ftCreationTime;
  a->ftLastAccessTime = w.ftLastAccessTime;
  a->ftLastWriteTime = w.ftLastWriteTime;
  a->nFileSizeHigh = w.nFileSizeHigh;
  a->nFileSizeLow = w.nFileSizeLow;
  a->dwReserved0 = w.dwReserved0;
  a->dwReserved1 = w.dwReserved1;
  store_path (a->cFileName, sizeof a->cFileName, w.cFileName);
  store_path (a->cAlternateFileName, sizeof a->cAlternateFileName,
              w.cAlternateFileName);
}

HANDLE WINAPI
WINFS::FindFirstFile (LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
  wpath w (lpFileName);
  WIN32_FIND_DATAW fd;
  HANDLE r = ::FindFirstFileW (w, &fd);
  if (r == INVALID_HANDLE_VALUE && askpass (lpFileName))
    r = ::FindFirstFileW (w, &fd);
  if (r != INVALID_HANDLE_VALUE)
    store_find_data (lpFindFileData, fd);
  return r;
}

BOOL WINAPI
WINFS::FindNextFile (HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
  WIN32_FIND_DATAW fd;
  *fd.cFileName = 0;
  BOOL r = (::FindNextFileW (hFindFile, &fd)
            || (GetLastError () == ERROR_MORE_DATA && *fd.cFileName));
  if (r)
    store_find_data (lpFindFileData, fd);
  else
    *lpFindFileData->cFileName = 0;
  return r;
}

static BOOL WINAPI
GetDiskFreeSpaceFAT32 (LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
                       LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                       LPDWORD lpTotalNumberOfClusters)
{
  char buf[PATH_MAX + 1];
  if (!lpRootPathName)
    {
      if (!GetCurrentDirectory (sizeof buf, buf))
        return 0;
      lpRootPathName = root_path_name (buf, buf);
    }

  dyn_handle hvwin32 (CreateFile ("\\\\.\\vwin32", 0, 0, 0, 0,
                                  FILE_FLAG_DELETE_ON_CLOSE, 0));
  if (!hvwin32.valid ())
    return 0;

  ExtGetDskFreSpcStruc dfs = {0};
  DIOC_REGISTERS regs = {0};
  regs.reg_EAX = 0x7303;
  regs.reg_ECX = sizeof dfs;
  regs.reg_EDX = DWORD (lpRootPathName);
  regs.reg_EDI = DWORD (&dfs);

  DWORD nbytes;
  if (!DeviceIoControl (hvwin32, VWIN32_DIOC_DOS_DRIVEINFO,
                        &regs, sizeof regs, &regs, sizeof regs,
                        &nbytes, 0)
      || regs.reg_Flags & X86_CARRY_FLAG)
    return 0;

  *lpSectorsPerCluster = dfs.SectorsPerCluster;
  *lpBytesPerSector = dfs.BytesPerSector;
  *lpNumberOfFreeClusters = dfs.AvailableClusters;
  *lpTotalNumberOfClusters = dfs.TotalClusters;

  return 1;
}

BOOL WINAPI
WINFS::GetDiskFreeSpace (LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
                         LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                         LPDWORD lpTotalNumberOfClusters)
{
  wpath w (lpRootPathName);
  BOOL r = ::GetDiskFreeSpaceW (w, lpSectorsPerCluster, lpBytesPerSector,
                                lpNumberOfFreeClusters, lpTotalNumberOfClusters);
  if (!r)
    {
      if (GetLastError () == ERROR_NOT_SUPPORTED)
        {
          *lpSectorsPerCluster = 1;
          *lpBytesPerSector = 4096;
        }
      else
        {
          if (!askpass (lpRootPathName))
            return 0;
          r = ::GetDiskFreeSpaceW (w, lpSectorsPerCluster, lpBytesPerSector,
                                   lpNumberOfFreeClusters, lpTotalNumberOfClusters);
          if (!r)
            return 0;
        }
    }

  if (GetDiskFreeSpaceEx)
    {
      if (!sysdep.WinNTp ()
          && GetDiskFreeSpaceFAT32 (lpRootPathName, lpSectorsPerCluster,
                                    lpBytesPerSector, lpNumberOfFreeClusters,
                                    lpTotalNumberOfClusters))
        return 1;

      uint64_t FreeBytesAvailableToCaller;
      uint64_t TotalNumberOfBytes;
      uint64_t TotalNumberOfFreeBytes;
      if (GetDiskFreeSpaceEx (w,
                              (PULARGE_INTEGER)&FreeBytesAvailableToCaller,
                              (PULARGE_INTEGER)&TotalNumberOfBytes,
                              (PULARGE_INTEGER)&TotalNumberOfFreeBytes))
        {
          DWORD blk = *lpSectorsPerCluster * *lpBytesPerSector;
          if (!blk)
            blk = 512;
          *lpTotalNumberOfClusters = DWORD (TotalNumberOfBytes / blk);
          *lpNumberOfFreeClusters = DWORD (TotalNumberOfFreeBytes / blk);
          r = 1;
        }
    }

  return r;
}

DWORD WINAPI
WINFS::internal_GetFileAttributes (LPCSTR lpFileName)
{
  wpath w (lpFileName);
  WINFS_CALL1 (DWORD, -1, lpFileName, GetFileAttributesW (w));
}

DWORD WINAPI
WINFS::GetFileAttributes (LPCSTR lpFileName)
{
  DWORD attr = internal_GetFileAttributes (lpFileName);
  if (attr == DWORD (-1) && GetLastError () != ERROR_INVALID_NAME)
    {
      WIN32_FIND_DATA fd;
      if (get_file_data (lpFileName, fd))
        attr = fd.dwFileAttributes;
    }
  return attr;
}

UINT WINAPI
WINFS::GetTempFileName (LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique, LPSTR lpTempFileName)
{
  wpath w (lpPathName), prefix (lpPrefixString);
  WCHAR buf[PATH_MAX + 1];
  UINT r = ::GetTempFileNameW (w, prefix, uUnique, buf);
  if (!r && askpass (lpPathName))
    r = ::GetTempFileNameW (w, prefix, uUnique, buf);
  if (r)
    store_path (lpTempFileName, PATH_MAX + 1, buf);
  return r;
}

BOOL WINAPI
WINFS::GetVolumeInformation (LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
                             DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                             LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                             LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
  wpath w (lpRootPathName);
  WCHAR vol[MAX_PATH + 1], fs[MAX_PATH + 1];
  BOOL r = ::GetVolumeInformationW (w, vol, numberof (vol), lpVolumeSerialNumber,
                                    lpMaximumComponentLength, lpFileSystemFlags,
                                    fs, numberof (fs));
  if (!r && askpass (lpRootPathName))
    r = ::GetVolumeInformationW (w, vol, numberof (vol), lpVolumeSerialNumber,
                                 lpMaximumComponentLength, lpFileSystemFlags,
                                 fs, numberof (fs));
  if (r)
    {
      if (lpVolumeNameBuffer)
        store_path (lpVolumeNameBuffer, nVolumeNameSize, vol);
      if (lpFileSystemNameBuffer)
        store_path (lpFileSystemNameBuffer, nFileSystemNameSize, fs);
    }
  return r;
}

HMODULE WINAPI
WINFS::LoadLibrary (LPCSTR lpLibFileName)
{
  wpath w (lpLibFileName);
  WINFS_CALL1 (HMODULE, NULL, lpLibFileName, LoadLibraryW (w));
}

static BOOL
move_file (LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
  wpath from (lpExistingFileName), to (lpNewFileName);
  WINFS_CALL2 (BOOL, FALSE, lpExistingFileName, lpNewFileName,
               MoveFileW (from, to));
}

BOOL WINAPI
WINFS::MoveFile (LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
{
  for (int retry = 0;; retry++)
    {
      if (move_file (lpExistingFileName, lpNewFileName))
        return 1;
      if (retry >= 3)
        return 0;
      Sleep (50);
    }
}

BOOL WINAPI
WINFS::RemoveDirectory (LPCSTR lpPathName)
{
  wpath w (lpPathName);
  WINFS_CALL1 (BOOL, FALSE, lpPathName, RemoveDirectoryW (w));
}

BOOL WINAPI
WINFS::SetFileAttributes (LPCSTR lpFileName, DWORD dwFileAttributes)
{
  wpath w (lpFileName);
  WINFS_CALL1 (BOOL, FALSE, lpFileName,
               SetFileAttributesW (w, dwFileAttributes));
}

DWORD WINAPI
WINFS::internal_GetFullPathName (LPCSTR lpFileName, DWORD nBufferLength,
                                 LPSTR lpBuffer, LPSTR *lpFilePart)
{
  WINFS_MAPSL (lpFileName);
  wpath w (lpFileName);
  WCHAR buf[PATH_MAX + 1], *part;
  DWORD l = ::GetFullPathNameW (w, numberof (buf), buf, &part);
  if (!l && askpass (lpFileName))
    l = ::GetFullPathNameW (w, numberof (buf), buf, &part);
  if (!l || l >= numberof (buf))
    return l;
  l = store_path (lpBuffer, nBufferLength, buf);
  if (lpFilePart)
    {
      *lpFilePart = 0;
      if (part)
        {
          WCHAR tem = *part;
          *part = 0;
          char dir[PATH_MAX + 1];
          *lpFilePart = lpBuffer + store_path (dir, numberof (dir), buf);
          *part = tem;
        }
    }
  return l;
}

BOOL WINAPI
WINFS::SetCurrentDirectory (LPCSTR lpPathName)
{
  WINFS_MAPSL (lpPathName);
  wpath w (lpPathName);
  WINFS_CALL1 (BOOL, FALSE, lpPathName, SetCurrentDirectoryW (w));
}

DWORD WINAPI
WINFS::GetFullPathName (LPCSTR path, DWORD size, LPSTR buf, LPSTR *name)
{
  DWORD l = internal_GetFullPathName (path, size, buf, name);
  if (!l || l >= size)
    return l;
  if (!dir_separator_p (*path) || !dir_separator_p (path[1]))
    return l;
  if (alpha_char_p (*buf & 0xff) && buf[1] == ':'
      && dir_separator_p (buf[2]) && dir_separator_p (buf[3]))
    {
      strcpy (buf, buf + 2);
      l -= 2;
      if (name && *name >= buf + 2)
        *name -= 2;
    }
  return l;
}

DWORD WINAPI
WINFS::WNetOpenEnum (DWORD dwScope, DWORD dwType, DWORD dwUsage,
                     LPNETRESOURCE lpNetResource, LPHANDLE lphEnum)
{
  if (!lpNetResource)
    return ::WNetOpenEnum (dwScope, dwType, dwUsage, lpNetResource, lphEnum);

  DWORD r = ::WNetOpenEnumA (dwScope, dwType, dwUsage, lpNetResource, lphEnum);
  if (r != NO_ERROR && askpass_noshare (lpNetResource->lpRemoteName))
    r = ::WNetOpenEnumA (dwScope, dwType, dwUsage, lpNetResource, lphEnum);
  return r;
}

DWORD WINAPI
WINFS::GetModuleFileName (HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
  WCHAR buf[PATH_MAX + 1];
  if (!::GetModuleFileNameW (hModule, buf, numberof (buf)))
    return 0;
  return store_path (lpFilename, nSize, buf);
}

DWORD WINAPI
WINFS::GetTempPath (DWORD nBufferLength, LPSTR lpBuffer)
{
  WCHAR buf[PATH_MAX + 1];
  if (!::GetTempPathW (numberof (buf), buf))
    return 0;
  return store_path (lpBuffer, nBufferLength, buf);
}

DWORD WINAPI
WINFS::GetCurrentDirectory (DWORD nBufferLength, LPSTR lpBuffer)
{
  WCHAR buf[PATH_MAX + 1];
  if (!::GetCurrentDirectoryW (numberof (buf), buf))
    return 0;
  return store_path (lpBuffer, nBufferLength, buf);
}

UINT WINAPI
WINFS::GetWindowsDirectory (LPSTR lpBuffer, UINT uSize)
{
  WCHAR buf[PATH_MAX + 1];
  if (!::GetWindowsDirectoryW (buf, numberof (buf)))
    return 0;
  return store_path (lpBuffer, uSize, buf);
}

UINT WINAPI
WINFS::GetSystemDirectory (LPSTR lpBuffer, UINT uSize)
{
  WCHAR buf[PATH_MAX + 1];
  if (!::GetSystemDirectoryW (buf, numberof (buf)))
    return 0;
  return store_path (lpBuffer, uSize, buf);
}

const char *WINAPI
WINFS::getenv (const char *name, char *buf, DWORD size)
{
  WCHAR wname[256], wbuf[PATH_MAX + 1];
  if (strlen (name) >= numberof (wname))
    return 0;
  s2u (wname, name);
  DWORD l = ::GetEnvironmentVariableW (wname, wbuf, numberof (wbuf));
  if (!l || l >= numberof (wbuf))
    return 0;
  return store_path (buf, size, wbuf) ? buf : 0;
}

FILE *WINAPI
WINFS::fopen (const char *path, const char *mode)
{
  wpath w (path);
  WCHAR wmode[16];
  if (strlen (mode) >= numberof (wmode))
    return 0;
  s2u (wmode, mode);
  return _wfopen (w, wmode);
}

int WINAPI
WINFS::get_file_data (const char *path, WIN32_FIND_DATA &fd)
{
  HANDLE h = FindFirstFile (path, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  FindClose (h);
  return 1;
}
