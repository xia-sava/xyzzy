;;; ファイラを開いたまま止める。lvprobe.py が一覧を読むための足場。
;;;
;;; 中身が変わると段どうしの比較が濁るので、置き場所が動かない `docs/` を開く。

(ignore-errors
  (filer (merge-pathnames "docs/" (si:system-root))))
