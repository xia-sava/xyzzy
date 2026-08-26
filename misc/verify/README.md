# 検証台

xyzzy を実際に起動して、外から挙動を確かめるための道具。単体テスト
（`unittest/`）が Lisp の中から見るのに対し、こちらは**プロセスの外から**、
Windows の API 越しに見る。文字コードや窓まわりのように、Lisp からは
見えないところを確かめるために使う。

## 画面を占有しない

すべて `rundesk.py` 経由で、**利用者の画面とは別のデスクトップ**で xyzzy を
起動する。窓は現れず、作業の邪魔をしない。起動したプロセスは Job Object に
入れてあるので、走らせた python が倒れても道連れに落ちる。

**別デスクトップではエラーダイアログが見えない。** 検証台の中で例外を外へ
逃がすと、誰にも見えない場所でダイアログが出たまま止まり、時間切れになる。
`ignore-errors` で全部包むこと。**読み込み時のエラーも同じ**なので、`si:` の
ような単一コロンで非エクスポートの名前を書いたり、不正な書式指定を書いたり
しないこと（そちらは `ignore-errors` の外側で起きる）。

## 座標は実画素で読む

相手は DPI に対応した窓を持つ。こちらが対応しないままだと `GetWindowRect` が
96dpi へ縮めた座標を返し、**コントロールから貰う矩形と桁が合わない**。
`dlgtext.py` を読み込んだ時点で `SetProcessDPIAware` を呼んでいるので、
python の検証台はすべてこれに従う。

## 走らせ方

作業ファイルの置き場は環境変数 `XYZZY_VERIFY_WORK` で決める。指定しないと
python 側は使い捨てのディレクトリを作り、Lisp 側はカレントに書く。

```bash
W=$(mktemp -d)

# Lisp の検証台
XYZZY_VERIFY_WORK="$(cygpath -w "$W")" \
  python misc/verify/rundesk.py <xyzzy.exe> -q -l misc/verify/pathcheck.l -kill
iconv -f CP932 -t UTF-8 -c < "$W/pathcheck.txt"

# python の検証台
XYZZY_VERIFY_WORK="$(cygpath -w "$W")" python misc/verify/longnamecheck.py <xyzzy.exe>
```

Lisp 側が書き出す報告は xyzzy の既定の符号なので、CP932 として読む。

## 中身

### 結果が合否で出るもの

| | 何を確かめるか |
|---|---|
| `pathcheck.l` | 13 の操作 × 4 種の名前（ascii／日本語／`résumé`／`寿司🍣`）でパスが通るか |
| `paircheck.l` | サロゲート対の内側にカーソルが入らないか。移動・削除・桁・上書き |
| `isearchkey.l` | キーを表す文字で isearch が抜けるか。押鍵は `unread-char` で押し戻す |
| `longpathcheck.py` | 260 文字を超えるパスを開く・書く・消す。深さを引数で変えられる |
| `longnamecheck.py` | 一区画の名前が長くても一覧に出るか。かなの個数を引数で変えられる |
| `cmdlinecheck.py` | 非 ASCII の名前をコマンドラインから開けるか。`CreateProcessW` で直接渡す |
| `ccscheck.l` | どの文字集合でその字を書けるか（`parse-char-encoding-*`） |

### 窓の中を読むもの

| | 何を読むか |
|---|---|
| `caretprobe.py` | カーソルの矩形。全角と同じ幅になっているかを画素で見る |
| `lvprobe.py` | 一覧（ListView）の項目 |
| `tabprobe.py` | タブの文字 |
| `ttprobe.py` | ツールチップの本文。載せずに取り出す |
| `menuprobe.py` | メニューの文字 |
| `uniprobe.py` | その窓が Unicode 窓か（`IsWindowUnicode`） |
| `fmtprobe.py` | 通知を A/W どちらで受けるつもりか（`WM_NOTIFYFORMAT`） |
| `hoverprobe.py` | カーソルを載せたときの挙動。別デスクトップなので利用者の手は動かない |

### 土台と道具

| | |
|---|---|
| `rundesk.py` | 別デスクトップを作ってコマンドを走らせる。全部これを通る |
| `dlgtext.py` | 窓の列挙・クラス名・文字取得。python の検証台が読み込む |
| `recompile.l` | site-lisp の `.lc` を `.l` から一括で作り直す。`XYZZY_LC_HOLD` と `XYZZY_SITE_LISP` で場所を渡す |
