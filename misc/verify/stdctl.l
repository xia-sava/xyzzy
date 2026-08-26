;;; 標準コントロールのサブクラス（src/stdctl.cc）を外から確かめるための
;;; ダイアログを出す。stdctlprobe.py がこれを -l で読ませ、リストボックス・
;;; コンボボックス・コンボボックスの中のエディットに C-n を投げて、選択が
;;; 動くかを見る。

(ignore-errors
  (dialog-box
   '(dialog 0 0 200 120
     (:caption "stdctl")
     (:font 9 "MS UI Gothic")
     (:control
      (:listbox list nil #x50a10111 7 7 80 60)
      (:combobox combo nil #x50210842 7 72 80 60)
      (:edit edit nil #x50810080 7 92 80 14)
      (:button IDOK "OK" #x50010001 143 7 50 14)
      (:button IDCANCEL "キャンセル" #x50010000 143 24 50 14)))
   (list (cons 'list '(("alpha") ("bravo") ("charlie") ("delta")))
         (cons 'combo '("one" "two" "three")))
   nil))
