;;; Run every unit test and write the report to a file, then let -kill end xyzzy.
;;;
;;; run-tests.bat leaves the report in a buffer for a human to read. Running the
;;; tests unattended needs the report on disk instead, so this writes it out.
;;;
;;; The tests live next to the running xyzzy, so the directory comes from
;;; si:system-root. Where to write the report comes from XYZZY_TEST_OUT;
;;; without it the report lands in the current directory.
;;;
;;; Kept to ASCII so that it reads the same whichever encoding is assumed.

(defun run-tests-output-path ()
  (let ((w (si:getenv "XYZZY_TEST_OUT")))
    (if w
        (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w)
      "test-results.txt")))

(let ((dir (merge-pathnames "unittest/" (si:system-root)))
      (out (run-tests-output-path)))
  (load-file (merge-pathnames "simple-test.l" dir))
  (let ((*error-on-overwriting-test* t))
    (dolist (test (sort (directory dir :absolute t :recursive t :wild "*-tests.l")
                        #'string-lessp))
      (load-test-file test)))
  (run-all-tests)
  (let ((buf (find-buffer "*Test Results*")))
    (when buf
      (save-excursion
        (set-buffer buf)
        (write-region (point-min) (point-max) out)))))
