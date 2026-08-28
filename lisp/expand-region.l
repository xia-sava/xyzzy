;;; -*- Mode: Lisp; Package: EDITOR -*-
;;;
;;; This file is part of xyzzy.
;;;
;;; kuwa72/xyzzy <https://github.com/kuwa72/xyzzy> の lisp/expand-region.l を
;;; もとにしている。ライセンスは同じ MIT。
;;;

;;; 選択範囲を意味のある単位で段階的に広げる。
;;;
;;;   単語 → シンボル → 引用符の中 → 引用符ごと
;;;        → 括弧の中 → 括弧ごと（外側の括弧へ、以下同様）
;;;        → 行 → 関数定義 → バッファ全体
;;;
;;; 候補を全部作っておき、今の範囲を真に含むものの中から最も小さいものを選ぶ。
;;; 段階を手続きとして並べるのに比べて、
;;;
;;;   * 候補の並びと実際の広がり方が食い違わない（必ず小さい順に進む）
;;;   * モードによって成り立たない段階（Lisp でない所の関数定義など）は
;;;     候補から抜けるだけで、飛ばす処理を書かなくてよい
;;;
;;; 候補の並び順は、大きさが同じになったときの優先順位としてだけ効く。

(provide "expand-region")

(in-package "editor")

(export '(expand-region contract-region
	  *expand-region-max-list-depth*))

(defvar *expand-region-max-list-depth* 20
  "括弧の候補を何段まで外へ辿るか。")

(defvar *expand-region-history* nil
  "広げてきた範囲のスタック。contract-region が戻る先。")

(defvar *expand-region-origin* nil
  "続けて広げるときに候補を作る基準の位置。")

(defvar *expand-region-last* nil
  "最後に置いた (バッファ 開始 終了)。今の範囲がこれと違えば、
間に別の操作が入ったものとして数え直す。")

;;; 広げた履歴は、増やしたカーソルごとに別のものを持つ。
(setq *multi-cursor-local-variables*
      (append *multi-cursor-local-variables*
	      '(*expand-region-history* *expand-region-origin*
		*expand-region-last*)))

(defun expand-region-current ()
  "今の範囲を (開始 . 終了) で返す。選択が無ければ点の位置だけ。"
  (if (get-selection-type)
      (let ((m (selection-mark))
	    (p (selection-point)))
	(cons (min m p) (max m p)))
    (cons (point) (point))))

(defun expand-region-set (region)
  "REGION を選択して、続きを数えられるように控える。"
  (goto-char (car region))
  (if (= (car region) (cdr region))
      (stop-selection)
    (progn
      (start-selection 2 nil)
      (goto-char (cdr region))))
  (setq *expand-region-last*
	(list (selected-buffer) (car region) (cdr region)))
  t)

(defun expand-region-chain-broken-p (current)
  "前に置いた範囲から動いていたら non-nil。"
  (not (and *expand-region-last*
	    (eq (first *expand-region-last*) (selected-buffer))
	    (= (second *expand-region-last*) (car current))
	    (= (third *expand-region-last*) (cdr current)))))

(defun expand-region-syntax-bounds (origin spec)
  "ORIGIN を含む「SPEC の文字が続く範囲」を返す。広がらなければ nil。
SPEC は skip-syntax-spec-forward の構文指定（\"w\" で単語、\"w_\" でシンボル）。"
  (save-excursion
    (goto-char origin)
    (skip-syntax-spec-backward spec)
    (let ((from (point)))
      (goto-char origin)
      (skip-syntax-spec-forward spec)
      (let ((to (point)))
	(and (< from to) (cons from to))))))

(defun expand-region-string-bounds (origin)
  "ORIGIN が文字列の中なら、引用符の中と引用符ごとの 2 つを返す。
parse-point-syntax は開始引用符では :string を返さず、中身と閉じ引用符で返す。
:string が続く範囲は「開始引用符の次から閉じ引用符まで」なので、
両端を 1 文字ずつずらして外側を得る。"
  (when (eq (parse-point-syntax origin) :string)
    (let ((from origin)
	  (to origin))
      (while (and (> from (point-min))
		  (eq (parse-point-syntax (1- from)) :string))
	(decf from))
      (while (and (< to (point-max))
		  (eq (parse-point-syntax to) :string))
	(incf to))
      (list (cons from (1- to))
	    (cons (1- from) to)))))

(defun expand-region-list-origin (origin)
  "括弧を辿る起点を返す。文字列の中からは backward-up-list が外へ出られないので、
その場合は開始引用符まで戻す。そこは既に文字列の外なので普通に辿れる。"
  (let ((bounds (expand-region-string-bounds origin)))
    (if bounds
	(car (second bounds))
      origin)))

(defun expand-region-list-bounds (origin0)
  "ORIGIN0 を囲む括弧を内側から外側へ辿り、括弧の中と括弧ごとを小さい順に返す。"
  (let ((result nil)
	(origin (expand-region-list-origin origin0)))
    (dotimes (i *expand-region-max-list-depth*)
      (let ((found (save-excursion
		     (goto-char origin)
		     (when (backward-up-list (1+ i) t)
		       (let ((open (point)))
			 (when (forward-sexp 1 t)
			   (cons open (point))))))))
	(if (null found)
	    (return)
	  (progn
	    (push (cons (1+ (car found)) (1- (cdr found))) result)
	    (push found result)))))
    (nreverse result)))

(defun expand-region-line-bounds (origin)
  (save-excursion
    (goto-char origin)
    (goto-bol)
    (let ((from (point)))
      (goto-eol)
      (cons from (point)))))

(defun expand-region-defun-bounds (origin)
  (save-excursion
    (goto-char origin)
    (when (beginning-of-defun)
      (let ((from (point)))
	(goto-char origin)
	(when (end-of-defun)
	  (cons from (point)))))))

(defun expand-region-candidates (origin)
  "ORIGIN を基準にした候補を、優先したい順に並べて返す。"
  (let ((cands nil))
    (dolist (c (list (expand-region-syntax-bounds origin "w")
		     (expand-region-syntax-bounds origin "w_")))
      (when c (push c cands)))
    (dolist (c (expand-region-string-bounds origin))
      (push c cands))
    (dolist (c (expand-region-list-bounds origin))
      (push c cands))
    (push (expand-region-line-bounds origin) cands)
    (let ((c (expand-region-defun-bounds origin)))
      (when c (push c cands)))
    (push (cons (point-min) (point-max)) cands)
    (nreverse cands)))

(defun expand-region-next (current origin)
  "CURRENT を真に含む候補のうち最も小さいものを返す。無ければ nil。"
  (let ((from (car current))
	(to (cdr current))
	(best nil))
    (dolist (c (expand-region-candidates origin))
      (when (and (<= (car c) from)
		 (>= (cdr c) to)
		 (or (< (car c) from) (> (cdr c) to))
		 (or (null best)
		     (< (- (cdr c) (car c)) (- (cdr best) (car best)))))
	(setq best c)))
    best))

(defun expand-region ()
  "選択範囲を 1 段階広げる。選択が無ければ点の位置から始める。"
  (interactive)
  (let ((current (expand-region-current)))
    (when (expand-region-chain-broken-p current)
      (setq *expand-region-history* nil
	    *expand-region-origin* (car current)))
    (let ((next (expand-region-next current
				    (or *expand-region-origin* (car current)))))
      (cond (next
	     (push current *expand-region-history*)
	     (expand-region-set next))
	    (t
	     (ding)
	     nil)))))

(defun contract-region ()
  "expand-region で広げた分を 1 段階戻す。"
  (interactive)
  (let ((current (expand-region-current)))
    (if (expand-region-chain-broken-p current)
	(progn (ding) nil)
      (let ((prev (pop *expand-region-history*)))
	(if prev
	    (expand-region-set prev)
	  (progn (ding) nil))))))

;;; `,` は終端マクロ文字なので、逃がさないと読み手が `C-` で切ってしまう。
(global-set-key #\C-. 'expand-region)
(global-set-key #\C-\, 'contract-region)
