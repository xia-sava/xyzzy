"""別デスクトップでタブにカーソルを載せ、ツールチップが出るかを見る。

    python hoverprobe.py <exe> [args...]

カーソルの位置はデスクトップごとに別なので、スレッドをそのデスクトップへ移して
から置けば、利用者の画面のカーソルは動かない。
"""
import ctypes, ctypes.wintypes as w, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
TCM_GETITEMCOUNT = 0x1304
TCM_GETTOOLTIPS = 0x1300 + 45
WM_MOUSEMOVE = 0x0200


def rect(h):
    r = w.RECT()
    u32.GetWindowRect(h, ctypes.byref(r))
    return r


def find_tab(hdesk):
    for top in D.enum(u32.EnumDesktopWindows, hdesk):
        if D.class_name(top) != "\u3000":
            continue
        for c in D.enum(u32.EnumChildWindows, top):
            if D.class_name(c) == "SysTabControl32":
                return top, c
    return None, None


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
        top = tab = None
        for _ in range(40):
            time.sleep(0.5)
            top, tab = find_tab(hdesk)
            if tab:
                break
        if not tab:
            print("タブコントロールが見つからない"); return 1
        tr = rect(top)
        print("本体の矩形 (%d,%d)-(%d,%d) 可視=%d"
              % (tr.left, tr.top, tr.right, tr.bottom, u32.IsWindowVisible(top)))
        # 配置し直させる
        u32.SetWindowPos(top, None, 0, 0, tr.right - tr.left - 1,
                         tr.bottom - tr.top - 1, 0x0014)  # NOMOVE|NOZORDER
        for _ in range(20):
            time.sleep(0.5)
            r = rect(tab)
            if r.right > r.left and r.bottom > r.top:
                break

        n = u32.SendMessageW(tab, TCM_GETITEMCOUNT, 0, 0)
        u32.SendMessageW.restype = ctypes.c_void_p
        tt = u32.SendMessageW(tab, TCM_GETTOOLTIPS, 0, 0)
        u32.SendMessageW.restype = ctypes.c_long
        print("タブ %d 枚 / ツールチップ hwnd=%s" % (n, hex(tt) if tt else "なし"))
        if not tt:
            return 1
        print("ツールチップは %s 窓" % ("W" if u32.IsWindowUnicode(tt) else "A"))

        r = rect(tab)
        print("タブの矩形 (%d,%d)-(%d,%d) 可視=%d"
              % (r.left, r.top, r.right, r.bottom, u32.IsWindowVisible(tab)))
        if r.right <= r.left or r.bottom <= r.top:
            print("タブに大きさが無いので載せられない"); return 1
        cy = (r.bottom - r.top) // 2
        for i in range(4):
            u32.SendMessageW(tab, WM_MOUSEMOVE, 0, (20 + i) | (cy << 16))
            time.sleep(0.4)
        for _ in range(10):
            time.sleep(0.3)
            if u32.IsWindowVisible(tt):
                tr = rect(tt)
                print("出た: %dx%d at (%d,%d)"
                      % (tr.right - tr.left, tr.bottom - tr.top, tr.left, tr.top))
                return 0
        print("出ない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
