"""Open files whose names need Unicode from the command line, and check
that xyzzy received the names unharmed.

    python cmdlinecheck.py <xyzzy.exe>

The names are built from code points here so that no shell, console code
page or source encoding sits between them and CreateProcessW.
"""
import importlib.util
import os
import pathlib
import shutil
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
RUNDESK = HERE / "rundesk.py"
# Scratch files stay out of the repository. Set XYZZY_VERIFY_WORK to keep them
# somewhere of your choosing instead of a directory that is thrown away.
WORK = pathlib.Path(os.environ.get("XYZZY_VERIFY_WORK")
                    or tempfile.mkdtemp(prefix="xyzzy-verify-"))

NAMES = [
    "ascii.txt",
    "\u65e5\u672c\u8a9e.txt",
    "r\u00e9sum\u00e9.txt",
    "\u5bff\u53f8\U0001f363.txt",
]

DUMP = """\
;;; Write the file name of every file buffer, as UTF-16 code units.
(with-open-file (o "%s" :direction :output :if-exists :supersede)
  (dolist (b (buffer-list))
    (let ((f (get-buffer-file-name b)))
      (when f
        (format o "~{~4,'0X~^ ~}~%%" (map 'list #'char-code f))))))
"""


def load_rundesk():
    spec = importlib.util.spec_from_file_location("rundesk", RUNDESK)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def units(s):
    """UTF-16 code units, which is how xyzzy holds a string."""
    b = s.encode("utf-16-le")
    return " ".join("%04X" % int.from_bytes(b[i:i + 2], "little")
                    for i in range(0, len(b), 2))


def main(argv):
    if not argv:
        sys.stderr.write("usage: cmdlinecheck.py <xyzzy.exe> [index...]\n")
        return 2
    exe = argv[0]
    global NAMES
    if argv[1:]:
        NAMES = [NAMES[int(i)] for i in argv[1:]]

    work = WORK / "cmdlinework"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir()
    for n in NAMES:
        (work / n).write_text("hello\n", encoding="utf-8")

    result = work / "cmdline-result.txt"
    dump = WORK / "cmdline-dump.l"
    dump.write_text(DUMP % str(result).replace("\\", "/"), encoding="ascii")

    cfg = tempfile.mkdtemp(prefix="wxp-")
    cfgdir = pathlib.Path(cfg) / "wxp"
    cfgdir.mkdir()
    os.environ["XYZZYCONFIGPATH"] = str(cfgdir)
    os.environ.setdefault("PROMPT", "$P$G")

    args = [exe, "-q"]
    args += [str(work / n) for n in NAMES]
    args += ["-l", str(dump), "-kill"]

    rc = load_rundesk().main(args)
    if not result.exists():
        sys.stderr.write("cmdlinecheck: result file not written (rc=%d)\n" % rc)
        return 1

    got = [l.strip().upper() for l in
           result.read_text(encoding="ascii").splitlines() if l.strip()]

    ng = 0
    for n in NAMES:
        tail = units("/" + n)
        label = n.encode("unicode_escape").decode("ascii")
        if any(g.endswith(tail) for g in got):
            print("OK   %s" % label)
        else:
            ng += 1
            print("NG   %s" % label)
            print("     want ...%s" % tail)
    for g in got:
        if not any(g.endswith(units("/" + n)) for n in NAMES):
            print("     unexpected ...%s" % g[-60:])
    print("NG=%d  (%d buffers)" % (ng, len(got)))

    shutil.rmtree(cfg, ignore_errors=True)
    return 1 if ng else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
