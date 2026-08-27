;;; Where do the extra cursors land, and do they follow the edits?
;;; The buffer is four lines; the third one is short on purpose so the
;;; column cannot always be kept.
;;;   line 1 "abcdef"  slots  0.. 5, newline  6
;;;   line 2 "ghijkl"  slots  7..12, newline 13
;;;   line 3 "mn"      slots 14..15, newline 16
;;;   line 4 "opqrst"  slots 17..22
;;; The work directory comes from XYZZY_VERIFY_WORK; the current one otherwise.
(defvar *mc-out*
  (let ((w (or (si:getenv "XYZZY_VERIFY_WORK") ".")))
    (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w)
            "/mccheck.txt")))

(defvar *mc-lines* '())
(defvar *mc-ng* 0)

(defun mc-flush ()
  (with-open-file (o *mc-out* :direction :output :if-exists :supersede)
    (dolist (l (reverse *mc-lines*)) (format o "~A~%" l))))

(defun mc-say (fmt &rest args)
  (push (apply #'format nil fmt args) *mc-lines*)
  (mc-flush))

(defun mc-check (tag want got)
  (if (equal want got)
      (mc-say "OK   ~A = ~A" tag got)
    (progn
      (incf *mc-ng*)
      (mc-say "NG   ~A = ~A (wanted ~A)" tag got want))))

(defun mc-fill (column)
  (multi-cursor-clear)
  (erase-buffer (selected-buffer))
  (insert "abcdef\nghijkl\nmn\nopqrst")
  (goto-char column))

;; The report stops where it breaks; a missing last line means it did.
(ignore-errors
 (let ((buf (create-new-buffer "probe")))
  (set-buffer buf)
  (setq need-not-save t)
  (setq auto-save nil)

  (mc-fill 0)
  (mc-check "nothing to list at first" nil (multi-cursor-list))

  ;; Growing downwards keeps the column of the cursor we started from.
  ;; Line 3 is too short for column 3, so the cursor sits at its end.
  (mc-fill 3)
  (mc-check "three added below" 3 (multi-cursor-add-below 3))
  (mc-check "kept the column, line 3 at its end" '(10 16 20) (multi-cursor-list))
  (mc-check "point did not move" 3 (point))

  ;; One at a time must land in the same places.
  (mc-fill 3)
  (multi-cursor-add-below)
  (multi-cursor-add-below)
  (multi-cursor-add-below)
  (mc-check "one at a time" '(10 16 20) (multi-cursor-list))

  ;; The other way round folds the last one back.
  (multi-cursor-add-above)
  (mc-check "the last one folded back" '(10 16) (multi-cursor-list))
  (multi-cursor-add-above)
  (multi-cursor-add-above)
  (mc-check "all folded back" nil (multi-cursor-list))

  ;; Growing upwards from the bottom line.
  (mc-fill 20)
  (mc-check "two added above" 2 (multi-cursor-add-above 2))
  (mc-check "upwards keeps the column too" '(10 16) (multi-cursor-list))

  ;; There is no line past either end.
  (mc-fill 3)
  (multi-cursor-add-below 3)
  (mc-check "nothing below the last line" nil (multi-cursor-add-below))
  (mc-fill 3)
  (mc-check "nothing above the first line" nil (multi-cursor-add-above))

  ;; Inserting before them pushes every cursor along.
  (mc-fill 3)
  (multi-cursor-add-below 3)
  (goto-char 0)
  (insert "XY")
  (mc-check "insertion at the head pushed them" '(12 18 22) (multi-cursor-list))

  ;; Deleting before them pulls them back.
  (goto-char 0)
  (delete-region 0 2)
  (mc-check "deletion at the head pulled them" '(10 16 20) (multi-cursor-list))

  ;; Cursors caught inside a deletion collapse into one.
  (mc-check "swallowed by one deletion" t (progn (delete-region 8 21)
                                                 (equal '(8) (multi-cursor-list))))

  ;; Clearing, and what clearing reports.
  (mc-fill 3)
  (multi-cursor-add-below 2)
  (mc-check "clear reports having done it" t (multi-cursor-clear))
  (mc-check "nothing left" nil (multi-cursor-list))
  (mc-check "clear reports nothing to do" nil (multi-cursor-clear))

  ;; Moving to another buffer drops them.
  (mc-fill 3)
  (multi-cursor-add-below 2)
  (let ((other (create-new-buffer "probe2")))
    (set-buffer other)
    (setq need-not-save t)
    (set-buffer buf))
  (mc-check "dropped on the way to another buffer" nil (multi-cursor-list))

  ;; The keys they are reached by.
  (mc-check "M-S-Down grows downwards" 'multi-cursor-add-below
            (lookup-keymap *global-keymap* #\M-S-Down))
  (mc-check "M-S-Up grows upwards" 'multi-cursor-add-above
            (lookup-keymap *global-keymap* #\M-S-Up))

  (mc-say "-- ~D NG" *mc-ng*)))
