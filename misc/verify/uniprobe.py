"""別デスクトップで起動したアプリの窓が Unicode 窓かどうかを訊く。

    python uniprobe.py <exe> [args...]

IsWindowUnicode はプロセスを跨いで訊けるので、相手の窓を触らずに判る。
"""
import ctypes, ctypes.wintypes as w, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32


def walk(h, depth, out):
    out.append("%s%-20s %08X %s" % ("  " * depth, D.class_name(h), h,
                                    "W" if u32.IsWindowUnicode(h) else "A"))
    for c in D.enum(u32.EnumChildWindows, h):
        if u32.GetParent(c) == h:
            walk(c, depth + 1, out)


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
            tops = [t for t in D.enum(u32.EnumDesktopWindows, hdesk)
                    if D.class_name(t) == "\u3000"]
            if not tops:
                continue
            time.sleep(1.5)
            out = []
            for t in tops:
                walk(t, 0, out)
            print("\n".join(out))
            return 0
        print("窓が見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
