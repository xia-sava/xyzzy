(deftest expand-region-word-bounds ()
  "単語の続く範囲を返すこと"
  (with-temp-buffer
    (insert "foo bar baz")
    (ed::expand-region-syntax-bounds 5 "w"))
  => (4 . 7))

(deftest expand-region-word-bounds-at-edge ()
  "単語の直後では、その単語を返すこと"
  (with-temp-buffer
    (insert "foo bar")
    (ed::expand-region-syntax-bounds 3 "w"))
  => (0 . 3))

(deftest expand-region-word-bounds-outside ()
  "単語から離れた位置では広がらないこと"
  (with-temp-buffer
    (insert "foo  bar")
    (ed::expand-region-syntax-bounds 4 "w"))
  => nil)

(deftest expand-region-symbol-bounds ()
  "シンボルはハイフンを跨いで一つになること"
  (with-temp-buffer
    (lisp-mode)
    (insert "(setq foo-bar 1)")
    (values (ed::expand-region-syntax-bounds 8 "w")
	    (ed::expand-region-syntax-bounds 8 "w_")))
  => (6 . 9)
  => (6 . 13))

(deftest expand-region-list-bounds-nested ()
  "括弧を内側から外側へ、括弧の中と括弧ごとの順で辿ること"
  (with-temp-buffer
    (lisp-mode)
    (insert "(a (b (c d) e) f)")
    (ed::expand-region-list-bounds 8))
  => ((7 . 10) (6 . 11) (4 . 13) (3 . 14) (1 . 16) (0 . 17)))

(deftest expand-region-list-bounds-none ()
  "括弧の外では候補が無いこと"
  (with-temp-buffer
    (lisp-mode)
    (insert "abc")
    (ed::expand-region-list-bounds 1))
  => nil)

(deftest expand-region-list-depth-limit ()
  "*expand-region-max-list-depth* より外へは辿らないこと"
  (with-temp-buffer
    (lisp-mode)
    (insert "((((x))))")
    (let ((ed::*expand-region-max-list-depth* 2))
      (length (ed::expand-region-list-bounds 4))))
  => 4)

(deftest expand-region-line-bounds ()
  "行の範囲を返すこと"
  (with-temp-buffer
    (insert "abc\ndefgh\nij")
    (ed::expand-region-line-bounds 6))
  => (4 . 9))

(deftest expand-region-next-picks-smallest ()
  "今の範囲を真に含む候補のうち最も小さいものを選ぶこと"
  (with-temp-buffer
    (lisp-mode)
    (insert "(aa (bb cc) dd)")
    (values (ed::expand-region-next '(5 . 7) 5)
	    (ed::expand-region-next '(5 . 10) 5)
	    (ed::expand-region-next '(4 . 11) 5)))
  => (5 . 10)
  => (4 . 11)
  => (1 . 14))

(deftest expand-region-next-stops-at-buffer ()
  "バッファ全体まで来たらそれ以上は無いこと"
  (with-temp-buffer
    (insert "abc")
    (ed::expand-region-next '(0 . 3) 1))
  => nil)

(deftest expand-region-next-needs-strict-growth ()
  "同じ大きさの候補は選ばないこと"
  (with-temp-buffer
    (insert "foo")
    (ed::expand-region-next '(0 . 3) 1))
  => nil)

(deftest expand-region-chain-broken-when-moved ()
  "控えた範囲から動いていたら数え直すと判定すること"
  (with-temp-buffer
    (insert "abcdef")
    (let ((ed::*expand-region-last* (list (selected-buffer) 1 3)))
      (values (ed::expand-region-chain-broken-p '(1 . 3))
	      (ed::expand-region-chain-broken-p '(2 . 3)))))
  => nil
  => t)

(deftest expand-region-chain-broken-without-history ()
  "何も控えていなければ数え直すと判定すること"
  (with-temp-buffer
    (insert "abcdef")
    (let ((ed::*expand-region-last* nil))
      (ed::expand-region-chain-broken-p '(1 . 3))))
  => t)

(deftest expand-region-history-is-per-cursor ()
  "広げた履歴が、カーソルごとに持つ変数として登録されていること"
  (values (and (member 'ed::*expand-region-history* ed::*multi-cursor-local-variables*) t)
	  (and (member 'ed::*expand-region-origin* ed::*multi-cursor-local-variables*) t)
	  (and (member 'ed::*expand-region-last* ed::*multi-cursor-local-variables*) t))
  => t
  => t
  => t)

(deftest expand-region-keys-are-bound ()
  "C-. と C-, に割り当たっていること"
  (values (lookup-key-command #\C-.)
	  (lookup-key-command #\C-\,))
  => ed::expand-region
  => ed::contract-region)
