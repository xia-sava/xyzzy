(deftest sjis-decode-valid ()
  "写せる組はこれまでどおり"
  (map 'list #'char-code
       (convert-encoding-to-internal *encoding-sjis* "a\x82\xa0\x83\x4a\xb1"))
  => (97 12354 12459 65393))

(deftest sjis-decode-unmappable-pair ()
  "CP932 に無い組は U+FFFD になり、2 バイト目は次の字として読む"
  (map 'list #'char-code
       (convert-encoding-to-internal *encoding-sjis* "A\x85\x40B"))
  => (65 65533 64 66))

(deftest sjis-decode-lead-eats-no-newline ()
  "先導バイトのうしろの改行を飲まない"
  (map 'list #'char-code
       (convert-encoding-to-internal *encoding-sjis* "\x82\x0a"))
  => (65533 10))

(deftest sjis-decode-resync ()
  "2 バイト目が先導バイトなら、そこから字が組み直る"
  (map 'list #'char-code
       (convert-encoding-to-internal *encoding-sjis* "\x85\x82\xa0"))
  => (65533 12354))

(deftest sjis-decode-lone-lead-at-eof ()
  "末尾に残った先導バイトは U+FFFD。元のファイルに無い字を作らない"
  (map 'list #'char-code
       (convert-encoding-to-internal *encoding-sjis* "\x82"))
  => (65533))

(deftest sjis-decode-trail-out-of-range ()
  "後続バイトとして正しくない値も飲み込まない"
  (map 'list #'char-code
       (convert-encoding-to-internal *encoding-sjis* "\x82\x7f"))
  => (65533 127))

(deftest sjis-fileio-unmappable-pair ()
  "ファイルから読む経路でも同じ"
  (with-temp-files (f)
    (with-open-file (o f :direction :output :encoding :binary)
      (princ "A\x85\x40B\x82\x0aC" o))
    (with-temp-buffer
      (let ((*expected-fileio-encoding* *encoding-sjis*))
        (insert-file-contents f))
      (map 'list #'char-code (buffer-substring (point-min) (point-max)))))
  => (65 65533 64 66 65533 10 67))
