#ifndef _vfs_h_
#define _vfs_h_

#include "cdecl.h"

/* 一区画を探して得たもの。API の WIN32_FIND_DATAA は名前を MAX_PATH バイトで
   持つが、UTF-8 では日本語が 86 文字までしか入らないので、名前の器を広げる */
struct find_data
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  char cFileName[NAME_MAX];
  char cAlternateFileName[14];
};

class WINFS
{
protected:
  typedef BOOL (WINAPI *GETDISKFREESPACEEX)(LPCWSTR, PULARGE_INTEGER,
                                            PULARGE_INTEGER, PULARGE_INTEGER);
  static const GETDISKFREESPACEEX GetDiskFreeSpaceEx;

  static DWORD WINAPI internal_GetFullPathName (LPCSTR lpFileName, DWORD nBufferLength,
                                                LPSTR lpBuffer, LPSTR *lpFilePart);
  static DWORD WINAPI internal_GetFileAttributes (LPCSTR lpFileName);
public:
  static char wfs_share_cache[MAX_PATH * 2];

  static void clear_share_cache () {*wfs_share_cache = 0;}

  static BOOL WINAPI CreateDirectory (LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
  static HANDLE WINAPI CreateFile (LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
  static BOOL WINAPI DeleteFile (LPCSTR lpFileName);
  static HANDLE WINAPI FindFirstFile (LPCSTR lpFileName, find_data *lpFindFileData);
  static BOOL WINAPI FindNextFile (HANDLE hFindFile, find_data *lpFindFileData);
  static BOOL WINAPI GetDiskFreeSpace (LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster,
                                       LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                                       LPDWORD lpTotalNumberOfClusters);
  static DWORD WINAPI GetFileAttributes (LPCSTR lpFileName);
  static DWORD WINAPI GetFullPathName (LPCSTR lpFileName, DWORD nBufferLength, LPSTR lpBuffer, LPSTR *lpFilePart);
  static UINT WINAPI GetTempFileName (LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique, LPSTR lpTempFileName);
  static BOOL WINAPI GetVolumeInformation (LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
                                           DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                                           LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                                           LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
  static HMODULE WINAPI LoadLibrary (LPCSTR lpLibFileName);
  static BOOL WINAPI MoveFile (LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
  static BOOL WINAPI RemoveDirectory (LPCSTR lpPathName);
  static BOOL WINAPI SetCurrentDirectory (LPCSTR lpPathName);
  static BOOL WINAPI SetFileAttributes (LPCSTR lpFileName, DWORD dwFileAttributes);
  static DWORD WINAPI WNetOpenEnum (DWORD dwScope, DWORD dwType, DWORD dwUsage,
                                    LPNETRESOURCE lpNetResource, LPHANDLE lphEnum);

  /* パスを組み立てて返すものも、同じ形の文字列で受け取る */
  static DWORD WINAPI GetModuleFileName (HMODULE hModule, LPSTR lpFilename, DWORD nSize);
  static DWORD WINAPI GetTempPath (DWORD nBufferLength, LPSTR lpBuffer);
  static DWORD WINAPI GetCurrentDirectory (DWORD nBufferLength, LPSTR lpBuffer);
  static UINT WINAPI GetWindowsDirectory (LPSTR lpBuffer, UINT uSize);
  static UINT WINAPI GetSystemDirectory (LPSTR lpBuffer, UINT uSize);
  static const char *WINAPI getenv (const char *name, char *buf, DWORD size);
  static FILE *WINAPI fopen (const char *path, const char *mode);

  static int WINAPI get_file_data (const char *, find_data &);
};

#endif /* _vfs_h_ */
