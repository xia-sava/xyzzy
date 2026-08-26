(deftest graphic-char-p-key ()
  "キーを表す文字は字ではない"
  (values (graphic-char-p #\Right)
          (graphic-char-p #\F1)
          (graphic-char-p #\M-a))
  => nil
  => nil
  => nil)

(deftest key-char-is-outside-char-code-limit ()
  "キーの符号は Unicode の外にある"
  (values (>= (char-code #\F1) char-code-limit)
          (>= (char-code #\M-a) char-code-limit)
          (>= (char-code #\Right) char-code-limit))
  => t
  => t
  => t)

(deftest graphic-char-p-surrogate ()
  "単独のサロゲートは字ではない。私用領域は字とする"
  (values (graphic-char-p (code-char #xd7ff))
          (graphic-char-p (code-char #xd800))
          (graphic-char-p (code-char #xdfff))
          (graphic-char-p (code-char #xe000)))
  => t
  => nil
  => nil
  => t)

(deftest graphic-char-p-c1 ()
  "C1 の制御文字は字ではない。NBSP は字"
  (values (graphic-char-p (code-char #x80))
          (graphic-char-p (code-char #x9f))
          (graphic-char-p (code-char #xa0)))
  => nil
  => nil
  => t)

(deftest graphic-char-p-ascii ()
  (values (graphic-char-p #\A)
          (graphic-char-p #\SPC)
          (graphic-char-p #\TAB)
          (graphic-char-p #\LFD)
          (graphic-char-p #\DEL)
          (graphic-char-p (code-char 0)))
  => t
  => t
  => nil
  => nil
  => nil
  => nil)

(deftest char-type-predicates-are-ascii ()
  "文字種の判定は ASCII の範囲だけを見る"
  (values (alpha-char-p #\a)
          (alpha-char-p (code-char #xe9))    ; é
          (alpha-char-p (code-char #xff41))  ; ａ
          (upper-case-p (code-char #xc0))    ; À
          (lower-case-p (code-char #xe0))    ; à
          (digit-char-p (code-char #xff11))  ; １
          (alphanumericp (code-char #xff21))); Ａ
  => t
  => nil
  => nil
  => nil
  => nil
  => nil
  => nil)
