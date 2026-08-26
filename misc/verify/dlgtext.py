"""別デスクトップで起動したプログラムのダイアログの文字を読む。

    python dlgtext.py <program> [args...]

撮影できない（BitBlt が通らない）場面でも、窓の文字は API で取れる。
利用者の画面には何も出さない。時間切れと Job Object の道連れ終了を必ず効かせる。
"""
import ctypes
import ctypes.wintypes as w
import sys
import time

u32 = ctypes.WinDLL("user32", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)

# 相手は DPI に対応した窓を持つ。こちらが対応しないままだと GetWindowRect が
# 96dpi へ縮めた座標を返し、コントロールから貰う矩形と桁が合わなくなる。
u32.SetProcessDPIAware()

DESKTOP_NAME = "xyzzy-offscreen"
GENERIC_ALL = 0x10000000
STARTF_USESTDHANDLES = 0x00000100
CREATE_SUSPENDED = 0x00000004
JobObjectExtendedLimitInformation = 9
JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000
WAIT_SECONDS = 30


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
                ("ActiveProcessLimit", w.DWORD),
                ("Affinity", ctypes.POINTER(ctypes.c_ulong)),
                ("PriorityClass", w.DWORD), ("SchedulingClass", w.DWORD)]


class JOBOBJECT_EXTENDED_LIMIT_INFORMATION(ctypes.Structure):
    _fields_ = [("BasicLimitInformation", JOBOBJECT_BASIC_LIMIT_INFORMATION),
                ("IoInfo", IO_COUNTERS), ("ProcessMemoryLimit", ctypes.c_size_t),
                ("JobMemoryLimit", ctypes.c_size_t),
                ("PeakProcessMemoryUsed", ctypes.c_size_t),
                ("PeakJobMemoryUsed", ctypes.c_size_t)]


ENUMPROC = ctypes.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)

# 既定の restype は 32 ビット整数なので、ハンドルが切り詰められる
u32.CreateDesktopW.restype = w.HANDLE
u32.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, ctypes.c_void_p,
                               w.DWORD, w.DWORD, ctypes.c_void_p]
u32.EnumDesktopWindows.argtypes = [w.HANDLE, ENUMPROC, w.LPARAM]
u32.EnumChildWindows.argtypes = [w.HWND, ENUMPROC, w.LPARAM]
k32.CreateJobObjectW.restype = w.HANDLE


def quote(a):
    return a if a and not any(c in a for c in ' \t"') else '"%s"' % a.replace('"', '\\"')


def window_text(hwnd):
    n = u32.GetWindowTextLengthW(hwnd)
    b = ctypes.create_unicode_buffer(n + 1)
    u32.GetWindowTextW(hwnd, b, n + 1)
    return b.value


def class_name(hwnd):
    b = ctypes.create_unicode_buffer(256)
    u32.GetClassNameW(hwnd, b, 256)
    return b.value


def enum(fn, arg):
    out = []

    def cb(h, _):
        out.append(h)
        return True

    fn(arg, ENUMPROC(cb), 0)
    return out


def dump(hwnd):
    print("=== ダイアログ: %s" % window_text(hwnd))
    for c in enum(u32.EnumChildWindows, hwnd):
        t = window_text(c)
        if t:
            print("  [%s] %s" % (class_name(c), t))


def main(argv):
    if not argv:
        sys.stderr.write("usage: dlgtext.py <program> [args...]\n")
        return 2

    hdesk = u32.CreateDesktopW(DESKTOP_NAME, None, None, 0, GENERIC_ALL, None)
    if not hdesk:
        sys.stderr.write("デスクトップを作れなかった err=%d\n" % ctypes.get_last_error())
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
        sys.stderr.write("起動できなかった err=%d\n" % ctypes.get_last_error())
        u32.CloseDesktop(hdesk)
        return 3

    if job:
        k32.AssignProcessToJobObject(job, pi.hProcess)
    k32.ResumeThread(pi.hThread)

    rc = 1
    try:
        for _ in range(WAIT_SECONDS * 2):
            time.sleep(0.5)
            tops = enum(u32.EnumDesktopWindows, hdesk)
            dialogs = [h for h in tops if class_name(h) == "#32770"]
            if dialogs:
                for h in dialogs:
                    dump(h)
                rc = 0
                break
            if k32.WaitForSingleObject(pi.hProcess, 0) == 0:
                print("ダイアログを出さずに終了")
                rc = 0
                break
        else:
            print("時間切れ。見えていた窓:")
            for h in enum(u32.EnumDesktopWindows, hdesk):
                print("  [%s] %s" % (class_name(h), window_text(h)))
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        k32.CloseHandle(pi.hThread)
        k32.CloseHandle(pi.hProcess)
        if job:
            k32.CloseHandle(job)
        u32.CloseDesktop(hdesk)
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
