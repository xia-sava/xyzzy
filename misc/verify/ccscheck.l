;;; Which character sets can hold each character?
;;; The work directory comes from XYZZY_VERIFY_WORK; the current one otherwise.
(defvar *cc-out*
  (let ((w (or (si:getenv "XYZZY_VERIFY_WORK") ".")))
    (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w)
            "/ccscheck.txt")))
(defvar *cc-lines* '())

(defun cc-say (fmt &rest args)
  (push (apply #'format nil fmt args) *cc-lines*)
  (with-open-file (o *cc-out* :direction :output :if-exists :supersede)
    (dolist (l (reverse *cc-lines*)) (format o "~A~%" l))))

(defun cc-try (label accept cps)
  (cc-say "~28A ~S" label
          (ignore-errors
           (parse-char-encoding-string
            accept (map 'string #'code-char cps)))))

;; one character at a time, accepting anything
(dolist (x '(("a"        #x0061)
             ("hiragana" #x3042)
             ("katakana" #x30A2)
             ("kanji"    #x6F22)
             ("hangul"   #xAC00)
             ("e-acute"  #x00E9)
             ("cyrillic" #x0416)
             ("greek"    #x03B1)
             ("halfkana" #xFF71)))
  (cc-try (car x) t (cdr x)))

(cc-say "")
;; the same kanji, but only one character set is on offer
(dolist (a '((:jisx0208) (:gb2312) (:ksc5601) (:big5) (:us-ascii)))
  (cc-try (format nil "kanji accept ~S" a) a '(#x6F22)))

(cc-say "")
;; a whole string, and things no legacy character set can hold
(cc-try "\"a\" + hiragana" t '(#x0061 #x3042))
(cc-try "surrogate half" t '(#xD83C))
(cc-try "hiragana + hangul" t '(#x3042 #xAC00))
(cc-try "hiragana accept ascii" '(:us-ascii) '(#x3042))

(cc-say "")
;; U+6C49 is the simplified form, U+4E2D is in every one of them
(dolist (a '((:gb2312) (:big5) (:jisx0208) (:ksc5601)))
  (cc-try (format nil "simplified accept ~S" a) a '(#x6C49)))
(dolist (a '((:gb2312) (:big5) (:jisx0208) (:ksc5601)))
  (cc-try (format nil "U+4E2D accept ~S" a) a '(#x4E2D)))
