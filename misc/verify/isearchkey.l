;;; Does a key that is not a character take isearch out of its loop?
;;; isearch reads through read-char *keyboard*, so the stroke is pushed back
;;; with unread-char instead of being typed. graphic-char-p decides what
;;; isearch-self-insert does with it: a key is not a character, so isearch must
;;; unread it and exit, leaving the stroke for the command loop to act on.
;;; If graphic-char-p answers t again, isearch appends the key to the search
;;; string and blocks on the next read-char. The report then stops after
;;; "-- calling isearch-forward" and rundesk.py times out; that is the failure.
;;; The work directory comes from XYZZY_VERIFY_WORK; the current one otherwise.
(defvar *ik-out*
  (let ((w (or (si:getenv "XYZZY_VERIFY_WORK") ".")))
    (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w)
            "/isearchkey.txt")))

(defvar *ik-lines* '())
(defvar *ik-ng* 0)

(defun ik-flush ()
  (with-open-file (o *ik-out* :direction :output :if-exists :supersede)
    (dolist (l (reverse *ik-lines*)) (format o "~A~%" l))))

(defun ik-say (fmt &rest args)
  (push (apply #'format nil fmt args) *ik-lines*)
  (ik-flush))

(defun ik-check (tag want got)
  (if (equal want got)
      (ik-say "OK   ~A = ~S" tag got)
    (progn
      (incf *ik-ng*)
      (ik-say "NG   ~A = ~S (wanted ~S)" tag got want))))

(ik-say "-- isearch is autoloaded, so it has to be asked for first")
(ignore-errors (require "isearch"))
(ik-check "isearch is loaded" t (and (fboundp 'isearch-forward) t))

(ignore-errors
 (let ((buf (create-new-buffer "probe")))
   (set-buffer buf)
   (setq need-not-save t)
   (setq auto-save nil)
   (insert "abc")
   (goto-char (point-min))

   (ik-check "a key is not a printing character" nil (graphic-char-p #\Right))
   (ik-check "and it has no binding of its own in isearch"
             nil (lookup-keymap *isearch-map* #\Right t))

   (unread-char #\Right *keyboard*)
   (ik-say "-- calling isearch-forward")
   (ik-check "isearch-forward returns instead of reading on"
             t (isearch-forward))
   (ik-check "the search string never took the key"
             nil ed::*isearch-history*)
   (ik-check "the key is left for the command loop"
             #x200006 (char-code (read-char *keyboard*)))

   (clear-reverse-region)
   (set-buffer-modified-p nil)))

(ik-say "")
(ik-say "NG = ~D" *ik-ng*)
(ik-flush)
