"""別デスクトップで起動した xyzzy のタブの文字を読む。"""
import ctypes, ctypes.wintypes as w, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
TCM_GETITEMCOUNT = 0x1304
TCM_GETITEMW = 0x133C


class TCITEMW(ctypes.Structure):
    _fields_ = [("mask", ctypes.c_uint), ("dwState", ctypes.c_uint),
                ("dwStateMask", ctypes.c_uint), ("pszText", w.LPWSTR),
                ("cchTextMax", ctypes.c_int), ("iImage", ctypes.c_int),
                ("lParam", w.LPARAM)]


def children(h):
    return D.enum(u32.EnumChildWindows, h)


# コモンコントロールはプロセスを跨いでポインタを解決しないので、
# 相手のアドレス空間に置いた領域を渡して読み返す
def tab_text(hwnd, i, cch=256):
    pid = w.DWORD()
    u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    k32.OpenProcess.restype = w.HANDLE
    hp = k32.OpenProcess(0x1F0FFF, False, pid)  # PROCESS_ALL_ACCESS
    if not hp:
        return "(プロセスを開けない)"
    k32.VirtualAllocEx.restype = ctypes.c_void_p
    size = ctypes.sizeof(TCITEMW) + cch * 2
    rem = k32.VirtualAllocEx(hp, None, size, 0x3000, 4)  # COMMIT|RESERVE, RW
    if not rem:
        k32.CloseHandle(hp)
        return "(領域を確保できない)"
    try:
        it = TCITEMW()
        it.mask = 1  # TCIF_TEXT
        it.pszText = ctypes.cast(ctypes.c_void_p(rem + ctypes.sizeof(TCITEMW)), w.LPWSTR)
        it.cchTextMax = cch
        k32.WriteProcessMemory(hp, ctypes.c_void_p(rem), ctypes.byref(it),
                               ctypes.sizeof(it), None)
        u32.SendMessageW(hwnd, TCM_GETITEMW, i, ctypes.c_void_p(rem))
        buf = ctypes.create_unicode_buffer(cch)
        k32.ReadProcessMemory(hp, ctypes.c_void_p(rem + ctypes.sizeof(TCITEMW)),
                              buf, cch * 2, None)
        return buf.value
    finally:
        k32.VirtualFreeEx(hp, ctypes.c_void_p(rem), 0, 0x8000)  # MEM_RELEASE
        k32.CloseHandle(hp)


def main():
    hdesk = u32.CreateDesktopW(D.DESKTOP_NAME, None, None, 0, D.GENERIC_ALL, None)
    si = D.STARTUPINFOW(); si.cb = ctypes.sizeof(si); si.lpDesktop = D.DESKTOP_NAME
    pi = D.PROCESS_INFORMATION()
    cmdline = ctypes.create_unicode_buffer(" ".join(D.quote(a) for a in sys.argv[1:]))
    if not k32.CreateProcessW(None, cmdline, None, None, True, D.CREATE_SUSPENDED,
                              None, None, ctypes.byref(si), ctypes.byref(pi)):
        print("起動できなかった"); return 3
    k32.ResumeThread(pi.hThread)
    try:
        for _ in range(40):
            time.sleep(0.5)
            for top in D.enum(u32.EnumDesktopWindows, hdesk):
                if D.class_name(top) != "\u3000":
                    continue
                tabs = [c for c in children(top)
                        if D.class_name(c) == "SysTabControl32"]
                if not tabs:
                    continue
                for t in tabs:
                    n = u32.SendMessageW(t, TCM_GETITEMCOUNT, 0, 0)
                    print("タブ %d 枚" % n)
                    for i in range(n):
                        print("  [%d] %s" % (i, tab_text(t, i)))
                return 0
        print("タブが見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
