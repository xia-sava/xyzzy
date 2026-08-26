"""別デスクトップで開いた一覧の項目と見出しを読む。

コモンコントロールはプロセスを跨いでポインタを解決しないので、相手の
アドレス空間に領域を置いて渡す。32 ビット側の並びを手で組む。
"""
import contextlib, ctypes, ctypes.wintypes as w, struct, sys, time
import dlgtext as D

u32, k32 = D.u32, D.k32
LVM_FIRST = 0x1000
LVM_GETITEMCOUNT = LVM_FIRST + 4
LVM_GETITEMW = LVM_FIRST + 75
LVM_SETSORTMARK = LVM_FIRST + 808   # xyzzy 独自。src/privctrl/privctrl.h
LVSM_DOWN = 0
LVSM_UP = 1
LVIF_TEXT = 1
HDM_FIRST = 0x1200
HDM_GETITEMCOUNT = HDM_FIRST + 0
HDM_GETITEMW = HDM_FIRST + 11
HDI_TEXT = 0x0002
HDI_FORMAT = 0x0004
CCH = 260
LVITEM_SIZE = 40          # mask..iIndent
HDITEM_SIZE = 48          # mask..state
HDITEM_FMT_OFFSET = 20

# 見出しの書式。並べ替えの矢印を自前で描いているか実装に任せているかがここに出る
HDF_JUSTIFY = ("LEFT", "RIGHT", "CENTER", "?")
HDF_BITS = ((0x8000, "OWNERDRAW"), (0x4000, "STRING"), (0x2000, "BITMAP"),
            (0x1000, "BITMAP_ON_RIGHT"), (0x0800, "IMAGE"),
            (0x0400, "SORTUP"), (0x0200, "SORTDOWN"))


@contextlib.contextmanager
def remote_buffer(hwnd, size):
    """その窓を持つプロセスに領域を確保し、(プロセス, 番地) を渡す。

    プロセスを開けなければ両方 None、確保できなければ番地だけ None。
    """
    pid = w.DWORD()
    u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    k32.OpenProcess.restype = w.HANDLE
    hp = k32.OpenProcess(0x1F0FFF, False, pid)
    if not hp:
        yield None, None
        return
    k32.VirtualAllocEx.restype = ctypes.c_void_p
    rem = k32.VirtualAllocEx(hp, None, size, 0x3000, 4)
    try:
        yield hp, rem
    finally:
        if rem:
            k32.VirtualFreeEx(hp, ctypes.c_void_p(rem), 0, 0x8000)
        k32.CloseHandle(hp)


def item_text(hwnd, i, sub=0):
    with remote_buffer(hwnd, 64 + CCH * 2) as (hp, rem):
        if not hp:
            return "(プロセスを開けない)"
        if not rem:
            return "(領域を確保できない)"
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


def find_header(hwnd):
    for c in D.enum(u32.EnumChildWindows, hwnd):
        if D.class_name(c) == "SysHeader32":
            return c
    return 0


def header_items(hwnd):
    """見出しの各列を (文字, 書式) で並べる。"""
    out = []
    n = u32.SendMessageW(hwnd, HDM_GETITEMCOUNT, 0, 0)
    with remote_buffer(hwnd, HDITEM_SIZE + CCH * 2) as (hp, rem):
        if not rem:
            return out
        text_at = rem + HDITEM_SIZE
        for i in range(n):
            hi = struct.pack("<IiIIiiiiiIII", HDI_TEXT | HDI_FORMAT, 0,
                             text_at, 0, CCH, 0, 0, 0, 0, 0, 0, 0)
            k32.WriteProcessMemory(hp, ctypes.c_void_p(rem), hi, HDITEM_SIZE, None)
            k32.WriteProcessMemory(hp, ctypes.c_void_p(text_at), b"\0" * (CCH * 2),
                                   CCH * 2, None)
            if not u32.SendMessageW(hwnd, HDM_GETITEMW, i, ctypes.c_void_p(rem)):
                out.append(("(読めない)", -1))
                continue
            got = ctypes.create_string_buffer(HDITEM_SIZE)
            k32.ReadProcessMemory(hp, ctypes.c_void_p(rem), got, HDITEM_SIZE, None)
            buf = ctypes.create_unicode_buffer(CCH)
            k32.ReadProcessMemory(hp, ctypes.c_void_p(text_at), buf, CCH * 2, None)
            out.append((buf.value,
                        struct.unpack_from("<i", got, HDITEM_FMT_OFFSET)[0]))
    return out


def format_name(fmt):
    if fmt < 0:
        return "(読めない)"
    return "|".join([HDF_JUSTIFY[fmt & 3]]
                    + [name for bit, name in HDF_BITS if fmt & bit])


def print_header(hwnd_header, note):
    items = header_items(hwnd_header)
    print("  見出し %08X%s:" % (hwnd_header, note))
    for i, (text, fmt) in enumerate(items):
        print("    <%d> %s   fmt=%08X %s"
              % (i, text, fmt & 0xFFFFFFFF, format_name(fmt)))
    return len(items)


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
                    h = find_header(c)
                    if h:
                        # 印を別の列の逆順へ移し、矢印が付け替わるかまで見る
                        if print_header(h, "") > 1:
                            u32.SendMessageW(c, LVM_SETSORTMARK, 1, LVSM_UP)
                            print_header(h, "（印を列 1 の逆順へ移した後）")
                    return 0
        print("一覧が見つからない")
        return 1
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


sys.exit(main())
