;;; パス周りを非 ASCII の名前で一通り通す。結果は符号位置で記録する。
;;; 作業場所は環境変数 XYZZY_VERIFY_WORK で受け取る。無ければカレント。
(defun pc-work-directory ()
  (let ((w (or (si:getenv "XYZZY_VERIFY_WORK") ".")))
    (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w) "/")))
(defvar *pc-out* (concat (pc-work-directory) "pathcheck.txt"))
(defvar *pc-root* (concat (pc-work-directory) "pcwork/"))
(defvar *pc-lines* '())
(defvar *pc-ng* 0)

(defun pc-say (fmt &rest args)
  (push (apply #'format nil fmt args) *pc-lines*)
  (with-open-file (o *pc-out* :direction :output :if-exists :supersede)
    (dolist (l (reverse *pc-lines*)) (format o "~A~%" l))))

;; 根の部分は共通なので落とし、残りを符号位置で見せる
(defun short (s)
  (if (stringp s)
      (let ((n (length *pc-root*)))
        (if (and (>= (length s) n) (string= *pc-root* s :end2 n))
            (subseq s n)
          s))
    nil))
(defun cp (s) (let ((x (short s)))
                (if x (format nil "~{~4,'0X~^ ~}" (map 'list #'char-code x)) "-")))
(defun mk (codes) (map 'string #'code-char codes))

(defun pc-check (label want got)
  (if (and (stringp got) (string= want got))
      (pc-say "  OK   ~A" label)
    (progn
      (incf *pc-ng*)
      (pc-say "  NG   ~A~24Twant=~A~%~24Tgot =~A" label (cp want) (cp got)))))

(defun pc-try (label thunk)
  (handler-case (funcall thunk)
    (error (c)
      (incf *pc-ng*)
      (pc-say "  NG   ~A  <~A>" label
              (substitute #\SPC #\LFD (si:*condition-string c)))
      nil)))

(defun pc-run (tag name)
  (pc-say "--- ~A  ~{~4,'0X~^ ~}" tag (map 'list #'char-code name))
  (let* ((dir (concat *pc-root* name "d/"))
         (file (concat dir name ".txt"))
         (file2 (concat dir name "2.txt")))
    (pc-try "create-directory" #'(lambda () (create-directory dir)))
    (pc-check "exist dir" dir (and (file-exist-p dir) dir))
    (pc-try "write" #'(lambda ()
                        (with-open-file (o file :direction :output
                                             :if-exists :supersede)
                          (format o "hello~%"))))
    (pc-check "exist file" file (and (file-exist-p file) file))
    (pc-check "directory" file
              (car (pc-try "directory" #'(lambda () (directory dir :absolute t)))))
    (pc-check "directory :wild" file
              (car (pc-try "wild" #'(lambda ()
                                      (directory dir :absolute t :wild "*.txt")))))
    (pc-check "truename" file (pc-try "truename" #'(lambda () (truename file))))
    (pc-check "namestring" file
              (pc-try "namestring" #'(lambda () (namestring file))))
    (let ((back (pc-try "read" #'(lambda ()
                                   (with-open-file (i file) (read-line i nil nil))))))
      (pc-check "content" "hello" back))
    (pc-check "buffer-file-name" file
              (pc-try "find-file" #'(lambda ()
                                      (find-file file)
                                      (prog1 (get-buffer-file-name)
                                        (kill-buffer (selected-buffer))))))
    (let ((tmp (pc-try "temp" #'(lambda () (make-temp-file-name "x" ".t" dir)))))
      (pc-check "temp-file-name" dir (and tmp (directory-namestring tmp)))
      (when (and tmp (file-exist-p tmp))
        (pc-try "delete temp" #'(lambda () (delete-file tmp)))))
    ;; その場所を作業ディレクトリにして外部プログラムを動かす。
    ;; 子の標準出力は端末の符号に落ちるので、印を残させて場所だけを見る
    (let ((mark (concat dir "mark.txt")))
      (pc-try "call-process"
              #'(lambda ()
                  (call-process "cmd /c echo ok>mark.txt" :exec-directory dir
                                :wait t :show :hide)))
      (pc-check "call-process cwd" mark (and (file-exist-p mark) mark))
      (when (file-exist-p mark) (delete-file mark)))
    (pc-try "rename-file" #'(lambda () (rename-file file file2)))
    (pc-check "exist renamed" file2 (and (file-exist-p file2) file2))
    (pc-try "delete-file" #'(lambda () (delete-file file2)))
    (pc-try "delete-directory" #'(lambda () (delete-directory dir)))))

(handler-case
    (progn
      (create-directory *pc-root* :if-exists :skip)
      (pc-run "ascii"    "plain")
      (pc-run "japanese" (mk '(#x65E5 #x672C #x8A9E)))
      (pc-run "accent"   (mk '(#x72 #xE9 #x73 #x75 #x6D #xE9)))
      (pc-run "emoji"    (mk '(#x5BFF #x53F8 #xD83C #xDF63)))
      (pc-say "")
      (pc-say "NG = ~D" *pc-ng*))
  (error (c) (pc-say "落ちた: ~A" (si:*condition-string c))))
