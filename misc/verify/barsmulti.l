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

(ignore-errors
  (progn
    (dotimes (i 40)
      (create-new-buffer (format nil "multi~D" i)))
    (create-buffer-bar)
    (show-tool-bar 'buffer-bar :top)
    (let ((m (si:getenv "XYZZY_MULTI_ROW")))
      (when m
        (setq *tab-bar-multi-row* t)
        (refresh-tool-bars)
        (when (equal m "off")
          (setq *tab-bar-multi-row* nil)
          (refresh-tool-bars))))))
