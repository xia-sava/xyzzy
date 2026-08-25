;;; -*-Lisp-*-
;;;
;;; This file is part of xyzzy.
;;;
;;; 配布されている最新版を確かめ、望まれれば入れ替えを始める。
;;;
;;; 実行中の xyzzy は自分自身を置き換えられないので、入れ替えは導入先に同梱した
;;; update-app.sh に任せ、こちらは起動してから終了する。update-app.sh は xyzzy が
;;; 消えるのを待ってから始める。

(provide "update")

(in-package "editor")

(export '(check-for-update
	  *update-manifest-url*
	  *update-applier-name*))

(defvar *update-manifest-url*
  "https://github.com/xia-sava/xyzzy/releases/download/latest/manifest.json")

(defvar *update-applier-name* "update-app.sh")

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

;; 入れ替えは bash で走る。無い環境では確認まではできても適用できないので、
;; 訊く前に確かめて、できないことをそのまま伝える。
(defun update-bash-available-p ()
  (zerop (update-run "bash --version")))

(defun update-start-applier ()
  (call-process (format nil "bash \"~A\" --from-app \"~A\""
			(update-applier-path) (update-install-directory))
		:no-std-handles t :show :show))

(defun update-tell (fmt &rest args)
  (message-box (apply #'format nil fmt args) "更新の確認" '(:ok :exclamation)))

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
     ((not (update-bash-available-p))
      (update-tell "新しい ~A が出ていますが、入れ替えに必要な bash が見つかりません。"
		   (car latest)))
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
