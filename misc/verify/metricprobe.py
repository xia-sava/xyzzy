"""バー類の寸法を外から読む。

    python metricprobe.py <exe> -q -l <...>/bars.l

ツールバーの大きさは xyzzy が自分の定数から積み上げて決めており
（`src/dockbar.cc` `tool_bar::calc_client_size`）、実装にボタンの寸法を
訊いていない。実装がボタンを大きくすると、バーが足りずに右端が切れる。
それを数で見るための道具。タブとステータスバーの区画も併せて出す。

タブのスピンは、縦置きのとき xyzzy が作り直して向きを変えている
（`src/dockbar.cc` `tab_bar::modify_spin`）。作り直せているかは
その窓の hInstance に出るので、向きと併せて示す。縦置きの絵は
`barsvert.l` を `-l` で読ませると出る。

段 0 と段 1 の出力を `diff` して読む。
"""
import ctypes, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
TB_BUTTONCOUNT = 0x0400 + 24
TB_GETITEMRECT = 0x0400 + 29
TB_GETBUTTONSIZE = 0x0400 + 58
TB_GETPADDING = 0x0400 + 87
TCM_GETITEMCOUNT = 0x1304
TCM_GETITEMRECT = 0x130A
SB_GETPARTS = 0x0400 + 6
SB_GETRECT = 0x0400 + 10
UDS_HORZ = 0x0040


def rect(h):
    r = D.w.RECT()
    u32.GetWindowRect(h, ctypes.byref(r))
    return r.left, r.top, r.right, r.bottom


def size(h):
    l, t, r, b = rect(h)
    return r - l, b - t


remote_rect = D.remote_rect


def show_toolbar(h):
    cx, cy = size(h)
    bs = u32.SendMessageW(h, TB_GETBUTTONSIZE, 0, 0)
    pad = u32.SendMessageW(h, TB_GETPADDING, 0, 0)
    n = u32.SendMessageW(h, TB_BUTTONCOUNT, 0, 0)
    print("ToolbarWindow32 %08X 窓 %dx%d ボタン寸法 %dx%d 余白 %dx%d 個数 %d"
          % (h, cx, cy, bs & 0xFFFF, (bs >> 16) & 0xFFFF,
             pad & 0xFFFF, (pad >> 16) & 0xFFFF, n))
    far = 0
    for i in range(n):
        r = remote_rect(h, TB_GETITEMRECT, i)
        if not r:
            print("  [%d] 取れない" % i)
            continue
        print("  [%d] (%d,%d)-(%d,%d)" % ((i,) + r))
        far = max(far, r[2], r[3])
    print("  ボタンの端 %d / 窓 %dx%d → %s"
          % (far, cx, cy, "収まっている" if far <= max(cx, cy) else "はみ出している"))


def show_tab(h, mods):
    cx, cy = size(h)
    n = u32.SendMessageW(h, TCM_GETITEMCOUNT, 0, 0)
    print("SysTabControl32 %08X 窓 %dx%d タブ %d 枚" % (h, cx, cy, n))
    for i in range(n):
        r = remote_rect(h, TCM_GETITEMRECT, i)
        print("  [%d] %s" % (i, "(%d,%d)-(%d,%d)" % r if r else "取れない"))
    for c in D.enum(u32.EnumChildWindows, h):
        if D.class_name(c) != "msctls_updown32":
            continue
        style = u32.GetWindowLongW(c, D.GWL_STYLE)
        print("  スピン %08X 向き %s 作り手 %s"
              % (c, "横" if style & UDS_HORZ else "縦",
                 D.window_module(mods, c)))


def show_status(h):
    cx, cy = size(h)
    n = u32.SendMessageW(h, SB_GETPARTS, 0, 0)
    print("msctls_statusbar32 %08X 窓 %dx%d 区画 %d" % (h, cx, cy, n))
    for i in range(n):
        r = remote_rect(h, SB_GETRECT, i)
        print("  [%d] %s" % (i, "(%d,%d)-(%d,%d)" % r if r else "取れない"))


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
            top = tops[0]
            mods = D.modules(pi.dwProcessId)
            print("本体 %dx%d" % size(top))
            for c in D.enum(u32.EnumChildWindows, top):
                cls = D.class_name(c)
                if cls == "ToolbarWindow32":
                    show_toolbar(c)
                elif cls == "SysTabControl32":
                    show_tab(c, mods)
                elif cls == "msctls_statusbar32":
                    show_status(c)
            return 0
        print("窓が見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
