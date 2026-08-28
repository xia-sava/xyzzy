"""鍵盤から届いた符号が、そのままバッファへ入るかを外から見る。

    python wcharprobe.py <exe> -q -l <...>/wcharprobe.l

窓は Unicode 窓なので `WM_CHAR` の wparam は UTF-16 の符号単位。これを直接
送るので実際の鍵盤は要らず、別デスクトップのまま確かめられる。

送るのは U+0080〜U+00FF を含む並び。ここは「一バイトの外部形式」と値が重なる
ため、符号位置をバイトとして復号する経路が残っていると別の字に化ける
（日本語配列では `*kbd-encoding*` が CP932 なので U+00D7 が U+FF97 になる）。

送り終えたら C-] を送る。`wcharprobe.l` がそれを受けてバッファの中身を
符号位置で `XYZZY_VERIFY_WORK` へ書き出す。
"""
import ctypes, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
WM_CHAR = 0x0102
TRIGGER = 29  # C-]

# 送る符号と、入るべき符号。ASCII・ラテン補助・仮名・漢字を混ぜる
CASES = [0x0041, 0x00D7, 0x00F7, 0x00B0, 0x00E9, 0x00A9, 0x00FF,
         0x3042, 0x4E00, 0xFF71]


def probe(top, work):
    for cp in CASES:
        u32.PostMessageW(top, WM_CHAR, cp, 1)
    time.sleep(1.0)
    u32.PostMessageW(top, WM_CHAR, TRIGGER, 1)
    time.sleep(1.5)

    try:
        with open(work + "/wcharprobe.txt", encoding="cp932") as f:
            lines = [x.strip() for x in f if x.strip()]
    except OSError:
        print("no report")
        return 1

    # 日本語を print すると端末の符号で化けるので、出すものは ASCII に限る
    got = [int(x) for x in lines if x.isdigit()]
    for x in lines:
        if not x.isdigit():
            print(x)

    ng = 0
    for i, want in enumerate(CASES):
        have = got[i] if i < len(got) else None
        ok = have == want
        if not ok:
            ng += 1
        print("sent U+%04X -> got %s  %s"
              % (want, "U+%04X" % have if have is not None else "(none)",
                 "ok" if ok else "NG"))
    if len(got) > len(CASES):
        ng += 1
        print("%d extra characters" % (len(got) - len(CASES)))
    print("NG = %d / %d" % (ng, len(CASES)))
    return 0 if ng == 0 else 1


def main():
    argv = sys.argv[1:]
    if not argv:
        sys.stderr.write("usage: wcharprobe.py <program> [args...]\n")
        return 2
    import os
    work = (os.environ.get("XYZZY_VERIFY_WORK") or ".").replace("\\", "/")

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
            return probe(tops[0], work)
        print("窓が見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


if __name__ == "__main__":
    sys.exit(main())
