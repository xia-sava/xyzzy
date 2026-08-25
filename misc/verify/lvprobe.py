"""別デスクトップで開いた一覧の項目を読む。

コモンコントロールはプロセスを跨いでポインタを解決しないので、相手の
アドレス空間に領域を置いて渡す。32 ビット側の並びを手で組む。
"""
import ctypes, ctypes.wintypes as w, struct, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
LVM_FIRST = 0x1000
LVM_GETITEMCOUNT = LVM_FIRST + 4
LVM_GETITEMW = LVM_FIRST + 75
LVIF_TEXT = 1
CCH = 260
LVITEM_SIZE = 40          # mask..iIndent


def item_text(hwnd, i, sub=0):
    pid = w.DWORD()
    u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    k32.OpenProcess.restype = w.HANDLE
    hp = k32.OpenProcess(0x1F0FFF, False, pid)
    if not hp:
        return "(プロセスを開けない)"
    k32.VirtualAllocEx.restype = ctypes.c_void_p
    rem = k32.VirtualAllocEx(hp, None, 64 + CCH * 2, 0x3000, 4)
    if not rem:
        k32.CloseHandle(hp); return "(領域を確保できない)"
    try:
        text_at = rem + 64
        it = struct.pack("<IiiIIIiiIi", LVIF_TEXT, i, sub, 0, 0,
                         text_at, CCH, 0, 0, 0)
        k32.WriteProcessMemory(hp, ctypes.c_void_p(rem), it, LVITEM_SIZE, None)
        k32.WriteProcessMemory(hp, ctypes.c_void_p(text_at), b"\0" * (CCH * 2),
                               CCH * 2, None)
        u32.SendMessageW(hwnd, LVM_GETITEMW, 0, ctypes.c_void_p(rem))
        buf = ctypes.create_unicode_buffer(CCH)
        k32.ReadProcessMemory(hp, ctypes.c_void_p(text_at), buf, CCH * 2, None)
        return buf.value
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
        for _ in range(50):
            time.sleep(0.5)
            for top in D.enum(u32.EnumDesktopWindows, hdesk):
                for c in D.enum(u32.EnumChildWindows, top):
                    if D.class_name(c) not in ("SysListView32", "SysListViewEx32"):
                        continue
                    n = u32.SendMessageW(c, LVM_GETITEMCOUNT, 0, 0)
                    if n <= 0:
                        continue
                    print("一覧 %08X: %d 項目 (親 %s)" % (c, n, D.window_text(top)))
                    for i in range(min(n, 12)):
                        t = item_text(c, i)
                        print("  [%d] %s   %s"
                              % (i, t, " ".join("U+%04X" % ord(ch) for ch in t)))
                    return 0
        print("一覧が見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
