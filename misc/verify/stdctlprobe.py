"""標準コントロールのサブクラス（src/stdctl.cc）が効いているかを外から見る。

    python stdctlprobe.py <exe> -q -l <...>/stdctl.l

`stdctl_hook_init` は `WH_CBT` で作られた窓を捕まえ、標準コントロールを一つずつ
差し替える。**クラスを登録したモジュールは合否ではない** — 差し替えは窓ごとなので、
クラスは `user32.dll` や `comctl32.dll` のままになる。ここに出るのは、いまどの実装が
標準コントロールを供給しているか。合否は実際の動きで見る。

見た目には出ず、落ちもせず、**キーが効かなくなるだけ**なので、クラスの出所と
実際の動きの両方を見る。動きは二つある。`*std-control-down-char*` による選択の
移動と、リストボックスを文字で選ぶ動き（`WM_PRIVATE_LISTBOX_CHAR` を親へ送る
経路。所有者描画のリストボックスは `LBS_HASSTRINGS` を落としてあるので、
実装まかせでは文字で選べない）。
"""
import ctypes, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
WM_CHAR = 0x0102
LB_GETCURSEL = 0x0188
LB_SETCURSEL = 0x0186
CB_GETCURSEL = 0x0147
CB_SETCURSEL = 0x014E
DOWN_CHAR = 0x0E          # *std-control-down-char* の既定は #\C-n


def find_dialog(hdesk, caption):
    for h in D.enum(u32.EnumDesktopWindows, hdesk):
        if D.class_name(h) == "#32770" and D.window_text(h) == caption:
            return h
    return None


def child_of_class(hwnd, name):
    for c in D.enum(u32.EnumChildWindows, hwnd):
        if D.class_name(c) == name:
            return c
    return None


def moves(hwnd, set_msg, get_msg, target):
    """target に C-n を投げて、選択が 0 から 1 へ動くかを見る。"""
    u32.SendMessageW(hwnd, set_msg, 0, 0)
    before = u32.SendMessageW(hwnd, get_msg, 0, 0)
    u32.SendMessageW(target, WM_CHAR, DOWN_CHAR, 1)
    after = u32.SendMessageW(hwnd, get_msg, 0, 0)
    return before, after


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
        dlg = None
        for _ in range(40):
            time.sleep(0.5)
            dlg = find_dialog(hdesk, "stdctl")
            if dlg:
                break
        if not dlg:
            print("ダイアログが見つからない")
            return 1
        time.sleep(1.0)

        mods = D.modules(pi.dwProcessId)
        lbx = child_of_class(dlg, "ListBox")
        cbx = child_of_class(dlg, "ComboBox")
        edt = child_of_class(dlg, "Edit")
        cbx_edt = child_of_class(cbx, "Edit") if cbx else None

        print("=== 標準コントロールを供給しているモジュール")
        for name, h in (("ダイアログ", dlg), ("ListBox", lbx), ("ComboBox", cbx),
                        ("Edit", edt), ("ComboBox の Edit", cbx_edt)):
            if h:
                print("  %-18s %s" % (name, D.class_module(mods, h)))
            else:
                print("  %-18s 見つからない" % name)

        print("=== C-n で選択が動くか")
        ng = 0
        for name, hwnd, set_msg, get_msg, target in (
                ("ListBox", lbx, LB_SETCURSEL, LB_GETCURSEL, lbx),
                ("ComboBox", cbx, CB_SETCURSEL, CB_GETCURSEL, cbx),
                ("ComboBox の Edit", cbx, CB_SETCURSEL, CB_GETCURSEL, cbx_edt)):
            if not hwnd or not target:
                print("  %-18s 見つからない" % name)
                ng += 1
                continue
            before, after = moves(hwnd, set_msg, get_msg, target)
            ok = after == before + 1
            print("  %-18s %d -> %d  %s" % (name, before, after,
                                            "OK" if ok else "NG"))
            if not ok:
                ng += 1

        print("=== リストボックスを文字で選べるか")
        if lbx:
            u32.SendMessageW(lbx, LB_SETCURSEL, 0, 0)
            u32.SendMessageW(lbx, WM_CHAR, ord("c"), 1)      # charlie は 3 番目
            after = u32.SendMessageW(lbx, LB_GETCURSEL, 0, 0)
            ok = after == 2
            print("  c を投げる        0 -> %d  %s" % (after, "OK" if ok else "NG"))
            if not ok:
                ng += 1
        else:
            print("  見つからない")
            ng += 1
        print("NG=%d" % ng)
        return 0 if ng == 0 else 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
