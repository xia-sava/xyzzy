"""別デスクトップで起動したアプリが、どの comctl32 で窓を作っているかを見る。

    python comctlprobe.py <exe> [args...]

**載っているだけでは判らない。** 5.82 も 6.x も `WinSxS` の下にあり、
comdlg32 などが自前のアクティブ化コンテキストで 6.x を引き込むので、
マニフェストが無くても両方が並ぶ。効いている版は**窓のクラスを登録した
モジュール**に出る。
"""
import ctypes, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32


def walk(h, depth, mods, out):
    out.append("%s%-22s %s" % ("  " * depth, D.class_name(h),
                               D.class_module(mods, h)))
    for c in D.enum(u32.EnumChildWindows, h):
        if u32.GetParent(c) == h:
            walk(c, depth + 1, mods, out)


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
                    if D.class_name(t) == "　"]
            if not tops:
                continue
            time.sleep(1.5)
            mods = D.modules(pi.dwProcessId)
            print("=== 載っている comctl32")
            for base, size, name, path in mods:
                if name.lower() == "comctl32.dll":
                    print("  %-16s %s" % (D.file_version(path), path))
            print("=== 窓のクラスを登録したモジュール")
            out = []
            for t in tops:
                walk(t, 1, mods, out)
            print("\n".join(out))
            return 0
        print("窓が見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
