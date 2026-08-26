;;; バッファバーを左端へ縦置きで出す。縦置きのタブとスピンを見るための足場。
;;;
;;; タブが一列に収まらないときだけスピン（`IDC_TAB_SPIN`）が現れるので、
;;; 窓の高さに収まらない数までバッファを作ってから出す。

(ignore-errors
  (progn
    (dotimes (i 60)
      (create-new-buffer (format nil "vert~D" i)))
    (create-buffer-bar)
    ;; 上へ出してから左へ移す。スピンの向きが変わる経路を通す
    (show-tool-bar 'buffer-bar :top)
    (show-tool-bar 'buffer-bar :left)))
