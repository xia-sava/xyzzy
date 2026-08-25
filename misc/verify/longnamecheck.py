"""Check that xyzzy can see file names that need more than 260 bytes of UTF-8.

    python longnamecheck.py <xyzzy.exe>

Windows allows 255 UTF-16 units per path component. In UTF-8 that is up to
765 bytes, so anything held in a 260 byte field is cut off well before the
name is actually too long.
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

# (label, name) -- lengths chosen around the old 260 byte limit
NAMES = [
    ("86 x kana (258 bytes, fits the old field)", "\u3042" * 86 + ".txt"),
    ("100 x kana (300 bytes)", "\u3044" * 100 + ".txt"),
    ("200 x kana (600 bytes)", "\u3046" * 200 + ".txt"),
    ("250 x kana (750 bytes, near the limit)", "\u3048" * 250 + ".txt"),
    ("120 x sushi (480 bytes, non-BMP)", "\U0001f363" * 120 + ".txt"),
]

DUMP = """\
;;; Write every name the directory lists, as UTF-16 code units.
(with-open-file (o "%s" :direction :output :if-exists :supersede)
  (dolist (n (directory "%s"))
    (format o "~{~4,'0X~^ ~}~%%" (map 'list #'char-code n))))
"""


def load_rundesk():
    spec = importlib.util.spec_from_file_location("rundesk", RUNDESK)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def units(s):
    b = s.encode("utf-16-le")
    return " ".join("%04X" % int.from_bytes(b[i:i + 2], "little")
                    for i in range(0, len(b), 2))


def main(argv):
    if not argv:
        sys.stderr.write("usage: longnamecheck.py <xyzzy.exe> [kana-count...]\n")
        return 2
    exe = argv[0]
    global NAMES
    if argv[1:]:
        NAMES = [("%s x kana (%d bytes)" % (n, int(n) * 3),
                  chr(0x3042 + i) * int(n) + ".txt")
                 for i, n in enumerate(argv[1:])]

    work = WORK / "longnamework"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir()
    made = []
    for label, n in NAMES:
        try:
            (work / n).write_text("hello\n", encoding="utf-8")
            made.append((label, n))
        except OSError as e:
            print("--   %s  (the file system refused it: %s)" % (label, e))

    result = work / "result.txt"
    dump = WORK / "longname-dump.l"
    dump.write_text(DUMP % (str(result).replace("\\", "/"),
                            str(work).replace("\\", "/") + "/"),
                    encoding="ascii")

    cfg = tempfile.mkdtemp(prefix="wxp-")
    cfgdir = pathlib.Path(cfg) / "wxp"
    cfgdir.mkdir()
    os.environ["XYZZYCONFIGPATH"] = str(cfgdir)
    os.environ.setdefault("PROMPT", "$P$G")

    rc = load_rundesk().main([exe, "-q", "-l", str(dump), "-kill"])
    if not result.exists():
        sys.stderr.write("longnamecheck: result file not written (rc=%d)\n" % rc)
        return 1

    got = set(l.strip().upper() for l in
              result.read_text(encoding="ascii").splitlines() if l.strip())

    ng = 0
    for label, n in made:
        if units(n) in got:
            print("OK   %s" % label)
        else:
            ng += 1
            print("NG   %s" % label)
    extra = len(got) - (len(made) - ng)
    print("NG=%d  (listed %d, of which %d unaccounted for)"
          % (ng, len(got), extra))

    shutil.rmtree(cfg, ignore_errors=True)
    return 1 if ng else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
