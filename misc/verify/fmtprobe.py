"""タブとツールチップが A/W どちらで通知するつもりかを訊く。"""
import ctypes, ctypes.wintypes as w, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
TCM_GETTOOLTIPS = 0x1300 + 45
WM_NOTIFYFORMAT = 0x0055
NF_QUERY, NF_REQUERY = 3, 4
NAME = {0: "答えない", 1: "A", 2: "W"}


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
            for t in D.enum(u32.EnumDesktopWindows, hdesk):
                if D.class_name(t) != "\u3000":
                    continue
                for c in D.enum(u32.EnumChildWindows, t):
                    if D.class_name(c) == "SysTabControl32":
                        top, tab = t, c
            if tab:
                break
        if not tab:
            print("タブコントロールが見つからない"); return 1
        time.sleep(2.0)
        u32.SendMessageW.restype = ctypes.c_void_p
        tt = u32.SendMessageW(tab, TCM_GETTOOLTIPS, 0, 0)
        u32.SendMessageW.restype = ctypes.c_long
        print("本体   %s窓  NF_QUERY=%s"
              % ("W" if u32.IsWindowUnicode(top) else "A",
                 NAME.get(u32.SendMessageW(top, WM_NOTIFYFORMAT, tab, NF_QUERY), "?")))
        print("タブ   %s窓  NF_QUERY=%s"
              % ("W" if u32.IsWindowUnicode(tab) else "A",
                 NAME.get(u32.SendMessageW(tab, WM_NOTIFYFORMAT, tt or 0, NF_QUERY), "?")))
        if tt:
            print("ﾁｯﾌﾟ   %s窓  NF_REQUERY=%s"
                  % ("W" if u32.IsWindowUnicode(tt) else "A",
                     NAME.get(u32.SendMessageW(tt, WM_NOTIFYFORMAT, tab, NF_REQUERY), "?")))
        return 0
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
