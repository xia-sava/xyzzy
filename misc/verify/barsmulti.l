;;; バッファバーを上へ出して段組みを見る足場。
;;;
;;; タブが一段に収まらないときだけ段が増えるので、窓の幅に収まらない数まで
;;; バッファを作る。一段で出してから `*tab-bar-multi-row*` を切り替えるので、
;;; 切り替えの経路も併せて踏む。
;;;
;;; 環境変数 `XYZZY_MULTI_ROW` で状態を選ぶ。
;;;
;;;   未設定  一段のまま。スピンが出る
;;;   1       多段にする
;;;   off     多段にしてから一段へ戻す
;;;
;;; `XYZZY_TAB_SELECT` にバッファ名を渡すと最後にそれを選ぶ。多段では選んだ
;;; タブの段が編集領域の側へ移るので、段の並び替わりを見るのに使う。
;;;
;;; `XYZZY_VERIFY_WORK` があれば、タブで選ばれたバッファの名前をそこの
;;; `tabclick.txt` へ書く。`tabclick.py` がタブを押したあとで読む。

(defvar *bm-out*
  (let ((w (si:getenv "XYZZY_VERIFY_WORK")))
    (and w
         (concat (map 'string #'(lambda (c) (if (char= c #\\) #\/ c)) w)
                 "/tabclick.txt"))))

(ignore-errors
  (progn
    (dotimes (i 40)
      (create-new-buffer (format nil "multi~D" i)))
    (when *bm-out*
      (setq-default *buffer-bar-hook*
                    #'(lambda (bp)
                        (set-buffer bp)
                        (with-open-file (o *bm-out* :direction :output
                                           :if-exists :supersede)
                          (format o "~A~%" (buffer-name bp))))))
    (create-buffer-bar)
    (show-tool-bar 'buffer-bar :top)
    (let ((m (si:getenv "XYZZY_MULTI_ROW")))
      (when m
        (setq *tab-bar-multi-row* t)
        (refresh-tool-bars)
        (when (equal m "off")
          (setq *tab-bar-multi-row* nil)
          (refresh-tool-bars))))
    (let ((b (si:getenv "XYZZY_TAB_SELECT")))
      (when b
        (let ((bp (find-buffer b)))
          (when bp
            (set-buffer bp)))))))
