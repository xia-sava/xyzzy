;;; Rebuild the .lc that were moved out of site-lisp, from their .l sources.
;;; Nothing may signal out of here: an error that escapes leaves xyzzy
;;; waiting where nobody can see it.
;;; Three directories come from the environment:
;;;   XYZZY_VERIFY_WORK  where the report is written
;;;   XYZZY_LC_HOLD      where the stale .lc were moved out to
;;;   XYZZY_SITE_LISP    the site-lisp whose sources are to be compiled
(defun rc-directory (name default)
  (let ((w (or (si:getenv name) default)))
    (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w) "/")))
(defvar *rc-out* (concat (rc-directory "XYZZY_VERIFY_WORK" ".") "recompile.txt"))
(defvar *rc-hold* (rc-directory "XYZZY_LC_HOLD" "."))
(defvar *rc-site* (rc-directory "XYZZY_SITE_LISP" "."))
(defvar *rc-lines* '())
(defvar *rc-ng* 0)

(defun rc-say (fmt &rest args)
  (push (apply #'format nil fmt args) *rc-lines*)
  (with-open-file (o *rc-out* :direction :output :if-exists :supersede)
    (dolist (l (reverse *rc-lines*)) (format o "~A~%" l))))

(rc-say "start")
(let ((held (or (ignore-errors
                 (directory *rc-hold* :absolute t :recursive t :wild "*.lc"))
                '())))
  (rc-say "-- ~D held" (length held))
  (dolist (lc held)
    (let* ((rel (subseq lc (length *rc-hold*)))
           (src (concat *rc-site* (subseq rel 0 (- (length rel) 2)) "l")))
      (cond ((not (file-exist-p src))
             (incf *rc-ng*)
             (rc-say "NO SOURCE ~A" rel))
            (t
             (multiple-value-bind (v e) (ignore-errors (byte-compile-file src))
               (declare (ignore v))
               (if e
                   (progn (incf *rc-ng*) (rc-say "NG   ~A -- ~A" rel e))
                 (rc-say "OK   ~A" rel)))))))
  (rc-say "")
  (rc-say "NG = ~D / ~D" *rc-ng* (length held)))
