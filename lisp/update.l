;;; -*-Lisp-*-
;;;
;;; This file is part of xyzzy.
;;;
;;; 配布されている最新版を確かめ、望まれれば入れ替えを始める。
;;;
;;; 実行中の xyzzy は自分自身を置き換えられないので、入れ替えは導入先に同梱した
;;; update-app.bat に任せ、こちらは起動してから終了する。update-app.bat は xyzzy が
;;; 消えるのを待ってから始め、済んだら開き直す。

(provide "update")

(in-package "editor")

(export '(check-for-update
	  update-rebuild-site-lisp
	  *update-manifest-url*
	  *update-applier-name*))

(defvar *update-manifest-url*
  "https://github.com/xia-sava/xyzzy/releases/download/latest/manifest.json")

(defvar *update-applier-name* "update-app.bat")

(defun update-slashify (path)
  (substitute #\/ #\\ path))

;; Windows 同梱のものを名指しする。PATH に別の curl が居ても取り違えない。
(defun update-curl-path ()
  (format nil "~A/System32/curl.exe"
	  (update-slashify (or (si:getenv "SystemRoot") "C:/Windows"))))

(defun update-run (command-line)
  "COMMAND-LINE を窓を出さずに実行し、終了コードを返す。"
  (let ((tmp (make-temp-file-name "upd" ".out")))
    (unwind-protect
	(call-process command-line :wait t :show :hide :output tmp :error tmp)
      (when (file-exist-p tmp)
	(ignore-errors (delete-file tmp))))))

;; 読む相手は自分たちが書いた manifest.json だけなので、平らな形に頼ってよい。
(defun update-json-value (path key)
  (with-open-file (s path :direction :input :if-does-not-exist nil)
    (when s
      (do ((line (read-line s nil) (read-line s nil)))
	  ((null line) nil)
	(when (string-match (format nil "\"~A\"[ \t]*:[ \t]*\"\\([^\"]*\\)\"" key) line)
	  (return (match-string 1)))))))

(defun update-fetch-manifest ()
  "配布中の版を (VERSION . COMMIT) で返す。取れなければ nil。"
  (let ((tmp (make-temp-file-name "upd" ".json"))
	(curl (update-curl-path)))
    (unwind-protect
	(when (and (file-exist-p curl)
		   (zerop (update-run (format nil "\"~A\" -fsSL \"~A\" -o \"~A\""
					      curl *update-manifest-url* tmp))))
	  (let ((version (update-json-value tmp "version")))
	    (and version (cons version (update-json-value tmp "commit")))))
      (when (file-exist-p tmp)
	(ignore-errors (delete-file tmp))))))

(defun update-install-directory ()
  (string-right-trim "/" (update-slashify (si:system-root))))

(defun update-applier-path ()
  (format nil "~A/~A" (update-install-directory) *update-applier-name*))

(defun update-start-applier ()
  (call-process (format nil "cmd /c \"\"~A\" /from-app \"~A\"\""
			(update-applier-path) (update-install-directory))
		:no-std-handles t :show :show))

(defun update-tell (fmt &rest args)
  (message-box (apply #'format nil fmt args) "更新の確認" '(:ok :exclamation)))

;; 入れ替えのとき退けた .lc を、対の .l から作り直す。作り直さなくても .l が読まれる
;; ので動きはするが、そのぶん起動が遅い。更新直後の開き直しで一度だけ呼ばれる。
(defun update-rebuild-site-lisp ()
  "退けられた site-lisp の .lc を作り直す。"
  (interactive)
  (let* ((hold (concat (string-right-trim "/" (update-install-directory)) ".lc-hold/"))
	 (site (format nil "~A/site-lisp/" (update-install-directory)))
	 (held (and (file-exist-p hold)
		    (ignore-errors
		     (directory hold :absolute t :recursive t :wild "*.lc"))))
	 (done 0) (ng 0))
    (cond
     ((null held)
      (message "作り直す .lc はありません"))
     (t
      (dolist (lc held)
	(let ((src (concat site (subseq lc (length hold) (- (length lc) 2)) "l")))
	  (if (not (file-exist-p src))
	      (incf ng)
	    (multiple-value-bind (v e) (ignore-errors (byte-compile-file src))
	      (declare (ignore v))
	      (if e (incf ng) (incf done))))))
      (message "site-lisp の .lc を ~D 個作り直しました~@[（~D 個は作り直せません）~]"
	       done (and (plusp ng) ng))))))

(defun check-for-update ()
  "配布されている最新版を確かめ、新しければ入れ替えるかどうかを訊く。"
  (interactive)
  (message "更新を確認しています...")
  (refresh-screen)
  (let ((latest (update-fetch-manifest)))
    (cond
     ((null latest)
      (update-tell "更新を確認できませんでした。~%通信できないか、配布元の情報を読めませんでした。"))
     ((<= (integer-version (car latest))
	  (integer-version (software-version)))
      (message-box (format nil "お使いの ~A が最新です。" (software-version))
		   "更新の確認" '(:ok :information)))
     ((not (file-exist-p (update-applier-path)))
      (update-tell "新しい ~A が出ていますが、入れ替えを行なう ~A が見つかりません。"
		   (car latest) *update-applier-name*))
     ((eq :yes (message-box
		(format nil "新しい ~A が出ています（いまは ~A）。~%~%~
			     いま入れ替えますか？~%~
			     xyzzy を終了してから入れ替え、終わったら開き直します。"
			(car latest) (software-version))
		"更新の確認" '(:yes-no :question)))
      (update-start-applier)
      (kill-xyzzy))
     (t
      (message "更新を取りやめました")))))
