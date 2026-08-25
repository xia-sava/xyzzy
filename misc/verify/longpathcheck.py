r"""Check that xyzzy can reach files whose whole path is longer than 260.

    python longpathcheck.py <xyzzy.exe> [depth...]

Windows caps a path at 260 characters unless the program says in its manifest
that it does not assume that limit.  The directories here are built through the
\\?\ prefix so that making them never depends on what is being measured.
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

SEG = "long-path-segment-0123456789abcdefghij"   # 38 chars + separator

PROBE = """\
;;; Report on one long path, a line per operation.  Nothing may signal out of
;;; here: an error that escapes leaves xyzzy waiting where nobody can see it.
(let* ((dir "%s")
       (src (concat dir "probe.txt"))
       (made (concat dir "made.txt")))
  (with-open-file (o "%s" :direction :output :if-exists :supersede)
    (labels ((p (tag fn)
               (multiple-value-bind (v e) (ignore-errors (funcall fn))
                 (if v
                     (format o "OK ~A~%%" tag)
                   (format o "NG ~A -- ~A~%%" tag
                           (if e (format nil "~A" e) "returned nil"))))))
      (format o "-- ~D characters~%%" (length src))
      (p "file-exist-p" #'(lambda () (file-exist-p src)))
      (p "directory" #'(lambda ()
                         (member "probe.txt" (directory dir) :test #'string=)))
      (p "read" #'(lambda ()
                    (with-open-file (i src) (string= (read-line i nil "") "hello"))))
      (p "write" #'(lambda ()
                     (with-open-file (w made :direction :output
                                        :if-exists :supersede)
                       (format w "made~%%"))
                     (file-exist-p made)))
      (p "truename" #'(lambda () (stringp (truename src))))
      (p "file-write-time" #'(lambda () (integerp (file-write-time src))))
      (p "delete-file" #'(lambda () (delete-file made)))
      ;; The recycle bin goes through the shell, which keeps its own limit.
      ;; Whatever happens, it must not report success while leaving the file.
      (p "recycle" #'(lambda ()
                       (with-open-file (w made :direction :output
                                          :if-exists :supersede)
                         (format w "made~%%"))
                       (and (delete-file made :recycle t)
                            (not (file-exist-p made))))))))
"""


def load_rundesk():
    spec = importlib.util.spec_from_file_location("rundesk", RUNDESK)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def build_tree(root, depth):
    """Make root/SEG/SEG/... and drop a file at the bottom.  Return the plain path."""
    plain = root
    for _ in range(depth):
        plain = plain / SEG
        os.makedirs("\\\\?\\" + str(plain), exist_ok=True)
    with open("\\\\?\\" + str(plain / "probe.txt"), "w", encoding="ascii") as f:
        f.write("hello\n")
    return plain


def run(exe, plain, tag):
    result = WORK / ("longpath-%s.txt" % tag)
    if result.exists():
        result.unlink()
    probe = WORK / ("longpath-probe-%s.l" % tag)
    probe.write_text(PROBE % (str(plain).replace("\\", "/") + "/",
                              str(result).replace("\\", "/")),
                     encoding="ascii")

    cfg = tempfile.mkdtemp(prefix="wxp-")
    cfgdir = pathlib.Path(cfg) / "wxp"
    cfgdir.mkdir()
    os.environ["XYZZYCONFIGPATH"] = str(cfgdir)
    os.environ.setdefault("PROMPT", "$P$G")
    rc = load_rundesk().main([exe, "-q", "-l", str(probe), "-kill"])
    shutil.rmtree(cfg, ignore_errors=True)

    if not result.exists():
        return None, rc
    # xyzzy reports its errors in the code page of the system.
    return result.read_text(encoding="cp932", errors="replace").splitlines(), rc


def main(argv):
    if not argv:
        sys.stderr.write("usage: longpathcheck.py <xyzzy.exe> [depth...]\n")
        return 2
    exe = argv[0]
    depths = [int(x) for x in argv[1:]] or [2, 7]

    work = WORK / "longpathwork"
    if work.exists():
        shutil.rmtree("\\\\?\\" + str(work.resolve()), ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)

    ng = 0
    for depth in depths:
        root = work / ("d%d" % depth)
        os.makedirs("\\\\?\\" + str(root.resolve()), exist_ok=True)
        plain = build_tree(root.resolve(), depth)
        lines, rc = run(exe, plain, "d%d" % depth)
        print("--- depth %d, %d characters ---" % (depth, len(str(plain)) + 10))
        if lines is None:
            print("NG   no result written (rc=%d)" % rc)
            ng += 1
            continue
        for l in lines:
            print("  " + l)
            if l.startswith("NG"):
                ng += 1
    print("NG=%d" % ng)
    return 1 if ng else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
