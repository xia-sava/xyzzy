"""ツールチップの本文を、載せずに取り出す。

TTM_GETTEXTW は callback 型の道具に対してその場でコールバックを起こすので、
カーソルを載せられない別デスクトップでも中身が判る。
"""
import ctypes, ctypes.wintypes as w, struct, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
TCM_GETITEMCOUNT = 0x1304
TCM_GETTOOLTIPS = 0x1300 + 45
TTM_GETTEXTA = 0x0400 + 11
TTM_GETTEXTW = 0x0400 + 56
V2_SIZE = 44          # lpReserved を含めない大きさ。これより大きいと撥ねられる
CCH = 512


def get_text(hwnd_tt, hwnd_tool, uid, msg, wide):
    pid = w.DWORD()
    u32.GetWindowThreadProcessId(hwnd_tt, ctypes.byref(pid))
    k32.OpenProcess.restype = w.HANDLE
    hp = k32.OpenProcess(0x1F0FFF, False, pid)
    if not hp:
        return "(プロセスを開けない)"
    k32.VirtualAllocEx.restype = ctypes.c_void_p
    nbytes = CCH * (2 if wide else 1)
    rem = k32.VirtualAllocEx(hp, None, 64 + nbytes, 0x3000, 4)
    if not rem:
        k32.CloseHandle(hp); return "(領域を確保できない)"
    try:
        text_at = rem + 64
        ti = struct.pack("<IIIiiiiiIII", V2_SIZE, 0, hwnd_tool, uid,
                         0, 0, 0, 0, 0, text_at, 0)
        k32.WriteProcessMemory(hp, ctypes.c_void_p(rem), ti, len(ti), None)
        k32.WriteProcessMemory(hp, ctypes.c_void_p(text_at), b"\0" * nbytes,
                               nbytes, None)
        u32.SendMessageW(hwnd_tt, msg, CCH, ctypes.c_void_p(rem))
        buf = (ctypes.create_unicode_buffer(CCH) if wide
               else ctypes.create_string_buffer(CCH))
        k32.ReadProcessMemory(hp, ctypes.c_void_p(text_at), buf, nbytes, None)
        return buf.value if wide else buf.value.decode("cp932", "replace")
    finally:
        k32.VirtualFreeEx(hp, ctypes.c_void_p(rem), 0, 0x8000)
        k32.CloseHandle(hp)


TTM_ENUMTOOLSW = 0x0400 + 58


def enum_tool(hwnd_tt, i):
    pid = w.DWORD()
    u32.GetWindowThreadProcessId(hwnd_tt, ctypes.byref(pid))
    k32.OpenProcess.restype = w.HANDLE
    hp = k32.OpenProcess(0x1F0FFF, False, pid)
    k32.VirtualAllocEx.restype = ctypes.c_void_p
    rem = k32.VirtualAllocEx(hp, None, 64, 0x3000, 4)
    try:
        ti = struct.pack("<IIIiiiiiIII", V2_SIZE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        k32.WriteProcessMemory(hp, ctypes.c_void_p(rem), ti, len(ti), None)
        r = u32.SendMessageW(hwnd_tt, TTM_ENUMTOOLSW, i, ctypes.c_void_p(rem))
        if not r:
            return None
        buf = ctypes.create_string_buffer(64)
        k32.ReadProcessMemory(hp, ctypes.c_void_p(rem), buf, 48, None)
        f = struct.unpack("<IIIiiiiiIII", buf.raw[:44])
        return ("uFlags=%08X hwnd=%08X uId=%d rect=(%d,%d,%d,%d) text=%08X"
                % (f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[9]))
    finally:
        k32.VirtualFreeEx(hp, ctypes.c_void_p(rem), 0, 0x8000)
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
        tab = None
        for _ in range(40):
            time.sleep(0.5)
            for t in D.enum(u32.EnumDesktopWindows, hdesk):
                if D.class_name(t) != "\u3000":
                    continue
                for c in D.enum(u32.EnumChildWindows, t):
                    if D.class_name(c) == "SysTabControl32":
                        tab = c
            if tab:
                break
        if not tab:
            print("タブコントロールが見つからない"); return 1
        time.sleep(2.0)
        n = u32.SendMessageW(tab, TCM_GETITEMCOUNT, 0, 0)
        u32.SendMessageW.restype = ctypes.c_void_p
        tt = u32.SendMessageW(tab, TCM_GETTOOLTIPS, 0, 0)
        u32.SendMessageW.restype = ctypes.c_long
        if not tt:
            print("ツールチップが無い"); return 1
        print("道具の数=%d" % u32.SendMessageW(tt, 0x0400 + 13, 0, 0))
        for i in range(4):
            e = enum_tool(tt, i)
            if not e:
                break
            print("道具[%d] %s" % (i, e))
        for i in range(n):
            print("[%d] W=%r  A=%r"
                  % (i,
                     get_text(tt, tab, i, TTM_GETTEXTW, True),
                     get_text(tt, tab, i, TTM_GETTEXTA, False)))
        return 0
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
