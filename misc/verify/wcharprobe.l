;;; Report what the keyboard actually inserted, as code points.
;;; wcharprobe.py sends WM_CHAR, then sends C-] to trigger this.
(defvar *wc-out*
  (let ((w (or (si:getenv "XYZZY_VERIFY_WORK") ".")))
    (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w)
            "/wcharprobe.txt")))

(defun wc-report ()
  (interactive)
  (ignore-errors
   (with-open-file (o *wc-out* :direction :output :if-exists :supersede)
     (let ((s (ignore-errors (buffer-substring (point-min) (point-max)))))
       (format o "count ~D~%" (length s))
       (dotimes (i (length s))
         (format o "~D~%" (char-code (char s i))))))))

;; C-] は自己挿入しないので、送った文字と混ざらない
(global-set-key (code-char 29) 'wc-report)
