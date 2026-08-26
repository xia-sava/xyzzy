;;; 検証台でバー類を出す。
;;;
;;; 検証台は利用者の設定を読ませないために -q で起動するので、ツールバーも
;;; バッファバーもファンクションバーも作られない。タブ・ツールチップ・
;;; ツールバーの寸法を見る道具は、これを -l で先に読ませてから使う。

(ignore-errors
  (progn
    (std-tool-bar)
    (show-tool-bar 'std-tool-bar)
    (create-buffer-bar)
    (show-tool-bar 'buffer-bar)
    (toggle-function-bar t)))
