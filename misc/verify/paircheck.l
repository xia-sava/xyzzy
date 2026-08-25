;;; Where does the caret land around a character that needs two buffer slots?
;;; U+1F363 is D83C DF63, so "a<pair>b" occupies four slots: 0 1 2 3.
;;; Everything is written as code units so the report needs no font.
;;; The work directory comes from XYZZY_VERIFY_WORK; the current one otherwise.
(defvar *pc-out*
  (let ((w (or (si:getenv "XYZZY_VERIFY_WORK") ".")))
    (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w)
            "/paircheck.txt")))

(defvar *pc-lines* '())
(defvar *pc-ng* 0)

(defun pc-flush ()
  (with-open-file (o *pc-out* :direction :output :if-exists :supersede)
    (dolist (l (reverse *pc-lines*)) (format o "~A~%" l))))

(defun pc-say (fmt &rest args)
  (push (apply #'format nil fmt args) *pc-lines*)
  (pc-flush))

(defun pc-check (tag want got)
  (if (equal want got)
      (pc-say "OK   ~A = ~A" tag got)
    (progn
      (incf *pc-ng*)
      (pc-say "NG   ~A = ~A (wanted ~A)" tag got want))))

(defun pc-units (s)
  (format nil "~{~4,'0X~^ ~}" (map 'list #'char-code s)))

(defun pc-text ()
  (pc-units (buffer-substring (point-min) (point-max))))

;; "a" U+1F363 "b"
(defun pc-fill ()
  (erase-buffer (selected-buffer))
  (insert (code-char #x61))
  (insert (code-char #xD83C))
  (insert (code-char #xDF63))
  (insert (code-char #x62))
  (goto-char (point-min)))

(let ((buf (create-new-buffer "probe")))
  (set-buffer buf)
  (setq need-not-save t)
  (setq auto-save nil)

  (pc-fill)
  (pc-say "-- buffer = ~A (4 slots)" (pc-text))

  ;; Stepping right must not stop between the two slots.
  (pc-fill)
  (forward-char 1)
  (pc-check "forward-char to just before the pair" 1 (point))
  (forward-char 1)
  (pc-check "forward-char over the pair" 3 (point))

  ;; And the same going left.
  (pc-fill)
  (goto-char (point-max))
  (backward-char 1)
  (pc-check "backward-char to just after the pair" 3 (point))
  (backward-char 1)
  (pc-check "backward-char over the pair" 1 (point))

  ;; Deleting forward from before the pair takes the whole character.
  (pc-fill)
  (goto-char 1)
  (ignore-errors (delete-char 1))
  (pc-check "delete-char takes both slots" "0061 0062" (pc-text))

  ;; Deleting backward from after the pair, likewise.
  (pc-fill)
  (goto-char 3)
  (ignore-errors (delete-char -1))
  (pc-check "delete-char -1 takes both slots" "0061 0062" (pc-text))

  ;; The buffer keeps code units, so char-after gives the leading half.
  (pc-fill)
  (goto-char 1)
  (pc-check "char-after at the pair is the leading half"
            #xD83C (char-code (char-after (point))))

  ;; Stepping over several slots at once must still land on a boundary.
  (pc-fill)
  (goto-char (point-min))
  (forward-char 2)
  (pc-check "forward-char 2 lands past the pair" 3 (point))

  ;; A count measured in slots keeps working: "a" plus the pair is 3 slots.
  (pc-fill)
  (goto-char (point-min))
  (forward-char 3)
  (pc-check "forward-char 3 lands where it always did" 3 (point))

  ;; Column arithmetic: the pair is one glyph, two columns wide.
  (pc-fill)
  (goto-char (point-max))
  (pc-check "current-column at end of line" 4 (current-column))

  ;; Landing by column must not fall between the slots.
  (pc-fill)
  (goto-column 2)
  (pc-check "goto-column 2 lands on the pair, not inside" 1 (point))

  ;; Typing over the pair should behave as it does over a full width
  ;; character: the slot is freed and the leftover column becomes a space.
  (pc-fill)
  (goto-char 1)
  (overwrite-char (code-char #x78))
  (pc-check "overwrite-char over the pair keeps the width"
            "0061 0078 0020 0062" (pc-text))

  ;; The same over a full width character, for comparison.
  (erase-buffer (selected-buffer))
  (insert (code-char #x61))
  (insert (code-char #x3042))
  (insert (code-char #x62))
  (goto-char 1)
  (overwrite-char (code-char #x78))
  (pc-check "overwrite-char over a full width character"
            "0061 0078 0020 0062" (pc-text))

  ;; quote-char feeds the two halves in one at a time, so overwriting with
  ;; a pair goes through overwrite-char twice.
  (erase-buffer (selected-buffer))
  (insert (code-char #x61)) (insert (code-char #x62)) (insert (code-char #x63))
  (goto-char (point-min))
  (overwrite-char (code-char #xD83C))
  (overwrite-char (code-char #xDF63))
  (pc-check "overwrite with a pair covers two columns"
            "d83c df63 0063" (pc-text))

  ;; Going forward then back must return to where it started.
  (pc-fill)
  (goto-char 1)
  (forward-char 1)
  (forward-char -1)
  (pc-check "forward then back returns" 1 (point))

  ;; Word motion must not stop between the slots either.
  (pc-fill)
  (goto-char (point-min))
  (forward-word 1)
  (pc-check "forward-word does not stop inside" t (/= 2 (point)))
  (goto-char (point-max))
  (forward-word -1)
  (pc-check "backward-word does not stop inside" t (/= 2 (point)))

  ;; Moving between lines by column.  Line 1 is "aa" pair "bb", so the
  ;; pair covers columns 2 and 3 and column 3 is inside it.
  (erase-buffer (selected-buffer))
  (insert (code-char #x61)) (insert (code-char #x61))
  (insert (code-char #xD83C)) (insert (code-char #xDF63))
  (insert (code-char #x62)) (insert (code-char #x62))
  (insert #\LFD)
  (dotimes (i 6) (insert (code-char #x78)))
  (goto-char (point-min))
  (pc-check "the pair covers two columns"
            6 (progn (goto-eol) (current-column)))

  (goto-char (point-max))
  (goto-column 3)
  (previous-line 1)
  (pc-check "previous-line with a goal inside the pair" 2 (current-column))
  (pc-check "and it lands on a slot boundary" 2 (point))

  (goto-char (point-min))
  (goto-column 3)
  (pc-check "goto-column 3 on the pair line stays outside" 2 (current-column))

  (set-buffer-modified-p nil)
  (pc-say "")
  (pc-say "NG = ~D" *pc-ng*)
  (pc-flush))
