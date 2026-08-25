"""Run a command on a separate desktop so its windows never touch the screen.

    python rundesk.py <program> [args...]

Windows は「デスクトップ」ごとにウィンドウの世界が分かれていて、切り替えていない
デスクトップの窓は利用者の画面に一切現れない。子プロセスは親のデスクトップを
引き継ぐので、cmd をここで起動すれば、その先の xyzzy も同じところに出る。

見えないところで固まると利用者が気づけないので、時間切れと Job Object の
道連れ終了を必ず効かせる。デスクトップを作れなかったときは、黙って見える方で
走らせずに失敗する。
"""
import ctypes
import ctypes.wintypes as w
import os
import sys

u32 = ctypes.WinDLL("user32", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)

DESKTOP_NAME = "xyzzy-offscreen"
GENERIC_ALL = 0x10000000
STARTF_USESTDHANDLES = 0x00000100
CREATE_SUSPENDED = 0x00000004
TIMEOUT_MS = 15 * 60 * 1000
JobObjectExtendedLimitInformation = 9
JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [("cb", w.DWORD), ("lpReserved", w.LPWSTR), ("lpDesktop", w.LPWSTR),
                ("lpTitle", w.LPWSTR), ("dwX", w.DWORD), ("dwY", w.DWORD),
                ("dwXSize", w.DWORD), ("dwYSize", w.DWORD),
                ("dwXCountChars", w.DWORD), ("dwYCountChars", w.DWORD),
                ("dwFillAttribute", w.DWORD), ("dwFlags", w.DWORD),
                ("wShowWindow", w.WORD), ("cbReserved2", w.WORD),
                ("lpReserved2", ctypes.c_void_p),
                ("hStdInput", w.HANDLE), ("hStdOutput", w.HANDLE),
                ("hStdError", w.HANDLE)]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", w.HANDLE), ("hThread", w.HANDLE),
                ("dwProcessId", w.DWORD), ("dwThreadId", w.DWORD)]


class IO_COUNTERS(ctypes.Structure):
    _fields_ = [("ReadOperationCount", ctypes.c_ulonglong),
                ("WriteOperationCount", ctypes.c_ulonglong),
                ("OtherOperationCount", ctypes.c_ulonglong),
                ("ReadTransferCount", ctypes.c_ulonglong),
                ("WriteTransferCount", ctypes.c_ulonglong),
                ("OtherTransferCount", ctypes.c_ulonglong)]


class JOBOBJECT_BASIC_LIMIT_INFORMATION(ctypes.Structure):
    _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong),
                ("PerJobUserTimeLimit", ctypes.c_longlong),
                ("LimitFlags", w.DWORD), ("MinimumWorkingSetSize", ctypes.c_size_t),
                ("MaximumWorkingSetSize", ctypes.c_size_t),
                ("ActiveProcessLimit", w.DWORD), ("Affinity", ctypes.POINTER(ctypes.c_ulong)),
                ("PriorityClass", w.DWORD), ("SchedulingClass", w.DWORD)]


class JOBOBJECT_EXTENDED_LIMIT_INFORMATION(ctypes.Structure):
    _fields_ = [("BasicLimitInformation", JOBOBJECT_BASIC_LIMIT_INFORMATION),
                ("IoInfo", IO_COUNTERS), ("ProcessMemoryLimit", ctypes.c_size_t),
                ("JobMemoryLimit", ctypes.c_size_t),
                ("PeakProcessMemoryUsed", ctypes.c_size_t),
                ("PeakJobMemoryUsed", ctypes.c_size_t)]


u32.CreateDesktopW.restype = w.HANDLE
u32.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, ctypes.c_void_p,
                               w.DWORD, w.DWORD, ctypes.c_void_p]
k32.CreateJobObjectW.restype = w.HANDLE


def quote(a):
    return a if a and not any(c in a for c in ' \t"') else '"%s"' % a.replace('"', '\\"')


def main(argv):
    if not argv:
        sys.stderr.write("usage: rundesk.py <program> [args...]\n")
        return 2

    hdesk = u32.CreateDesktopW(DESKTOP_NAME, None, None, 0, GENERIC_ALL, None)
    if not hdesk:
        sys.stderr.write("rundesk: デスクトップを作れなかった err=%d\n"
                         % ctypes.get_last_error())
        return 3

    job = k32.CreateJobObjectW(None, None)
    if job:
        info = JOBOBJECT_EXTENDED_LIMIT_INFORMATION()
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        k32.SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                    ctypes.byref(info), ctypes.sizeof(info))

    si = STARTUPINFOW()
    si.cb = ctypes.sizeof(si)
    si.lpDesktop = DESKTOP_NAME
    si.dwFlags = STARTF_USESTDHANDLES
    si.hStdInput = k32.GetStdHandle(-10)
    si.hStdOutput = k32.GetStdHandle(-11)
    si.hStdError = k32.GetStdHandle(-12)
    pi = PROCESS_INFORMATION()

    cmdline = ctypes.create_unicode_buffer(" ".join(quote(a) for a in argv))
    ok = k32.CreateProcessW(None, cmdline, None, None, True, CREATE_SUSPENDED,
                            None, None, ctypes.byref(si), ctypes.byref(pi))
    if not ok:
        sys.stderr.write("rundesk: 起動できなかった err=%d\n" % ctypes.get_last_error())
        u32.CloseDesktop(hdesk)
        return 3

    if job:
        k32.AssignProcessToJobObject(job, pi.hProcess)
    k32.ResumeThread(pi.hThread)

    rc = k32.WaitForSingleObject(pi.hProcess, TIMEOUT_MS)
    if rc != 0:
        sys.stderr.write("rundesk: 時間切れで打ち切った\n")
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        code = w.DWORD(1)
    else:
        code = w.DWORD()
        k32.GetExitCodeProcess(pi.hProcess, ctypes.byref(code))

    k32.CloseHandle(pi.hThread)
    k32.CloseHandle(pi.hProcess)
    if job:
        k32.CloseHandle(job)
    u32.CloseDesktop(hdesk)
    return code.value


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
