"""別デスクトップで起動した xyzzy のメニュー項目の文字を読む。"""
import ctypes, ctypes.wintypes as w, sys, time
sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
import dlgtext as D

u32 = D.u32
k32 = D.k32
u32.GetMenu.restype = w.HMENU
u32.GetSubMenu.restype = w.HMENU
u32.GetMenuStringW.argtypes = [w.HMENU, ctypes.c_uint, w.LPWSTR, ctypes.c_int, ctypes.c_uint]


def item(hmenu, i):
    b = ctypes.create_unicode_buffer(512)
    u32.GetMenuStringW(hmenu, i, b, 512, 0x400)  # MF_BYPOSITION
    return b.value


def main():
    hdesk = u32.CreateDesktopW(D.DESKTOP_NAME, None, None, 0, D.GENERIC_ALL, None)
    si = D.STARTUPINFOW()
    si.cb = ctypes.sizeof(si)
    si.lpDesktop = D.DESKTOP_NAME
    pi = D.PROCESS_INFORMATION()
    cmdline = ctypes.create_unicode_buffer(" ".join(D.quote(a) for a in sys.argv[1:]))
    if not k32.CreateProcessW(None, cmdline, None, None, True, D.CREATE_SUSPENDED,
                              None, None, ctypes.byref(si), ctypes.byref(pi)):
        print("起動できなかった err=%d" % ctypes.get_last_error()); return 3
    k32.ResumeThread(pi.hThread)
    try:
        for _ in range(40):
            time.sleep(0.5)
            for h in D.enum(u32.EnumDesktopWindows, hdesk):
                if D.class_name(h) != "\u3000":
                    continue
                hm = u32.GetMenu(h)
                if not hm:
                    continue
                n = u32.GetMenuItemCount(hm)
                print("メニュー段数=%d" % n)
                for i in range(n):
                    print("  [%d] %s" % (i, item(hm, i)))
                    sub = u32.GetSubMenu(hm, i)
                    if sub:
                        for j in range(min(u32.GetMenuItemCount(sub), 40)):
                            t = item(sub, j)
                            if t:
                                print("      - %s" % t)
                return 0
        print("メニューが見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
