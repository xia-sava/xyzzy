"""タブを押して、段の並びが動かないかを外から見る。

    python tabclick.py --tab 35 <exe> -q -l <...>/barsmulti.l

多段のとき、コモンコントロールは選ばれたタブの段を編集領域の側へ寄せる。
xyzzy はそれを避けるためにコントロールの選択を最終項目へ固定し、本当の
選択を自分で持つ（`src/dockbar.cc` `tab_bar::fix_rows`）。固定できて
いれば、どのタブを押しても項目の矩形は変わらない。

押すのは `WM_LBUTTONDOWN` と `WM_LBUTTONUP` を直接送る形なので、実際の
マウスは動かず、別デスクトップのまま確かめられる。どのバッファが選ばれた
かは `*buffer-bar-hook*` が `XYZZY_VERIFY_WORK` へ書き出す。
"""
import ctypes, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
TCM_GETITEMCOUNT = 0x1304
TCM_GETITEMRECT = 0x130A
TCM_GETROWCOUNT = 0x132C
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001


def item_rects(h, n):
    return [D.remote_rect(h, TCM_GETITEMRECT, i) for i in range(n)]


def rows_of(rects):
    """段の上端を、現れた順に並べる。"""
    tops = []
    for r in rects:
        if r and r[1] not in tops:
            tops.append(r[1])
    return tops


def show(tag, rects):
    print("%s 段の上端 %s" % (tag, rows_of(rects)))
    for i, r in enumerate(rects):
        print("  [%d] %s" % (i, "(%d,%d)-(%d,%d)" % r if r else "取れない"))


def click(h, r):
    x = (r[0] + r[2]) // 2
    y = (r[1] + r[3]) // 2
    lparam = (y << 16) | (x & 0xFFFF)
    u32.SendMessageW(h, WM_LBUTTONDOWN, MK_LBUTTON, lparam)
    u32.SendMessageW(h, WM_LBUTTONUP, 0, lparam)
    return x, y


def probe(top, index):
    tabs = [c for c in D.enum(u32.EnumChildWindows, top)
            if D.class_name(c) == "SysTabControl32"]
    if not tabs:
        print("タブが見つからない")
        return 1
    h = tabs[0]
    n = u32.SendMessageW(h, TCM_GETITEMCOUNT, 0, 0)
    rows = u32.SendMessageW(h, TCM_GETROWCOUNT, 0, 0)
    print("SysTabControl32 %08X タブ %d 枚 %d 段" % (h, n, rows))
    if index < 0 or index >= n:
        print("[%d] は範囲の外" % index)
        return 2

    before = item_rects(h, n)
    show("押す前", before)

    x, y = click(h, before[index])
    print("押した [%d] (%d,%d)" % (index, x, y))
    time.sleep(1.0)

    after = item_rects(h, n)
    show("押した後", after)
    same = before == after
    print("段の並び: %s" % ("動いていない" if same else "動いた"))
    return 0 if same else 1


def main():
    argv = sys.argv[1:]
    index = 0
    if len(argv) >= 2 and argv[0] == "--tab":
        index = int(argv[1])
        argv = argv[2:]
    if not argv:
        sys.stderr.write("usage: tabclick.py [--tab N] <program> [args...]\n")
        return 2

    hdesk = u32.CreateDesktopW(D.DESKTOP_NAME, None, None, 0, D.GENERIC_ALL, None)
    si = D.STARTUPINFOW(); si.cb = ctypes.sizeof(si); si.lpDesktop = D.DESKTOP_NAME
    pi = D.PROCESS_INFORMATION()
    cmdline = ctypes.create_unicode_buffer(" ".join(D.quote(a) for a in argv))
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
            return probe(tops[0], index)
        print("窓が見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
