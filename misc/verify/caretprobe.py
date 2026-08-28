r"""How wide is the caret standing on each character?

    python caretprobe.py <xyzzy.exe>

GetGUIThreadInfo hands back the caret rectangle of another process, so this
needs no screen: the editor runs on a desktop of its own.

The line is "a" + U+3042 + "b" + U+1F363 + "c".  A full width character gets a
caret two cells wide; a surrogate pair is also two cells on screen, so it must
get the same.
"""
import ctypes
import ctypes.wintypes as w
import os
import pathlib
import sys
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
# Scratch files stay out of the repository. Set XYZZY_VERIFY_WORK to keep them
# somewhere of your choosing instead of a directory that is thrown away.
WORK = pathlib.Path(os.environ.get("XYZZY_VERIFY_WORK")
                    or tempfile.mkdtemp(prefix="xyzzy-verify-"))
sys.path.insert(0, str(HERE))
import dlgtext as D

u32, k32 = D.u32, D.k32


class GUITHREADINFO(ctypes.Structure):
    _fields_ = [("cbSize", w.DWORD), ("flags", w.DWORD),
                ("hwndActive", w.HWND), ("hwndFocus", w.HWND),
                ("hwndCapture", w.HWND), ("hwndMenuOwner", w.HWND),
                ("hwndMoveSize", w.HWND), ("hwndCaret", w.HWND),
                ("rcCaret", w.RECT)]


SCRIPT = """\
(setq *normal-caret-shape* 0)
(let ((buf (create-new-buffer "probe")))
  (set-buffer buf)
  (setq need-not-save t)
  (setq auto-save nil)
  (insert (code-char #x61))
  (insert (code-char #x3042))
  (insert (code-char #x62))
  (insert (code-char #xD83C))
  (insert (code-char #xDF63))
  (insert (code-char #x63))
  (insert (code-char #xD7))
  (goto-char %d)
  (set-buffer-modified-p nil))
;; The caret exists from the moment the frame appears, so reading it too early
;; measures the scratch buffer instead. Say when the line is really in place.
(with-open-file (o "%s" :direction :output :if-exists :supersede)
  (princ "ok" o))
"""

# (label, point) -- slots are a(0) U+3042(1) b(2) high(3) low(4) c(5) U+00D7(6)
SPOTS = [
    ("on an ascii letter", 0, 1),
    ("on a full width character", 1, 2),
    ("on a surrogate pair", 3, 2),
    # U+00D7 is below U+0100 yet the japanese font draws it full width, so the
    # caret must follow the columns on screen, not the value of the code point.
    ("on an ambiguous width character", 6, 2),
]


def caret_width(exe, point, cfg):
    script = WORK / ("caret-%d.l" % point)
    ready = WORK / ("caret-%d.ready" % point)
    if ready.exists():
        ready.unlink()
    script.write_text(SCRIPT % (point, ready.as_posix()), encoding="ascii")

    hdesk = u32.CreateDesktopW(D.DESKTOP_NAME, None, None, 0, D.GENERIC_ALL, None)
    # GetGUIThreadInfo only answers about threads on our own desktop.
    u32.SetThreadDesktop(hdesk)
    si = D.STARTUPINFOW()
    si.cb = ctypes.sizeof(si)
    si.lpDesktop = D.DESKTOP_NAME
    pi = D.PROCESS_INFORMATION()
    args = [exe, "-q", "-l", str(script)]
    cmdline = ctypes.create_unicode_buffer(" ".join(D.quote(a) for a in args))
    if not k32.CreateProcessW(None, cmdline, None, None, True, D.CREATE_SUSPENDED,
                              None, None, ctypes.byref(si), ctypes.byref(pi)):
        u32.CloseDesktop(hdesk)
        return None
    k32.ResumeThread(pi.hThread)
    try:
        for _ in range(60):
            time.sleep(0.5)
            if ready.exists():
                break
        else:
            return None

        gti = GUITHREADINFO()
        gti.cbSize = ctypes.sizeof(gti)
        for _ in range(20):
            time.sleep(0.2)
            if u32.GetGUIThreadInfo(pi.dwThreadId, ctypes.byref(gti)) \
               and gti.hwndCaret:
                r = gti.rcCaret
                if r.right > r.left:
                    return r.right - r.left
        return None
    finally:
        k32.TerminateProcess(pi.hProcess, 1)
        k32.WaitForSingleObject(pi.hProcess, 5000)
        u32.CloseDesktop(hdesk)


def main(argv):
    if not argv:
        sys.stderr.write("usage: caretprobe.py <xyzzy.exe>\n")
        return 2
    exe = argv[0]
    cfg = tempfile.mkdtemp(prefix="wxp-")
    (pathlib.Path(cfg) / "wxp").mkdir()
    os.environ["XYZZYCONFIGPATH"] = str(pathlib.Path(cfg) / "wxp")
    os.environ.setdefault("PROMPT", "$P$G")

    cell = None
    ng = 0
    for label, point, want_cells in SPOTS:
        px = caret_width(exe, point, cfg)
        if px is None:
            print("NG   %s -- no caret found" % label)
            ng += 1
            continue
        if cell is None:            # the first spot is one cell by definition
            cell = px
        cells = px / cell
        ok = abs(cells - want_cells) < 0.01
        print("%s   %-28s %3d px = %.2f cells (wanted %d)"
              % ("OK" if ok else "NG", label, px, cells, want_cells))
        if not ok:
            ng += 1
    print("NG=%d" % ng)
    return 1 if ng else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
