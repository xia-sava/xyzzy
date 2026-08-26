# 開発の手引き

ビルドと検証のやり方、そして踏みやすい罠。守るべき原則は直下の `CLAUDE.md` に、
自分の PC でしか成り立たない話は各自の手元に置く。

## ビルドと検証の通し

```bash
misc/dev.sh all           # ビルド → バイトコンパイル → テスト
misc/dev.sh build         # 個別に叩く
misc/dev.sh bytecompile
misc/dev.sh test
```

**`bytecompile` と `test` は xyzzy を起動するが、別デスクトップで走るので画面には
出ない。** `misc/verify/rundesk.py` が `CreateDesktop` ＋ `CreateProcess(lpDesktop=...)`
で切り替えていないデスクトップを作り、そこで cmd を起動する。子プロセスは親の
デスクトップを引き継ぐので xyzzy も従う。デスクトップを作れなかったときは、
見える方へ落とさずに失敗する。

**例外は `test visible`。** `InternetExplorer.Application` を起動する OLE の 4 件だけは
対話デスクトップを要求するので、別デスクトップでは `CO_E_SERVER_EXEC_FAILURE` で
落ちる。`test visible` は**その 4 件だけ**を画面で走らせる。既定の実行と合わせて
全件になる。全件を画面で走らせたいときだけ `test visible all`。**どちらも画面を
占有するので断ってから。**

期待値は **ビルド 0 エラー 0 警告**、バイトコンパイル 130、
**`test` で 550/554**（OLE 4 件は上記のとおり落ちる）、**`test visible` で 4/4**。
警告が出たら回帰なので放置しないこと。

バイトコンパイルの数は `dev.sh` が `lisp/*.lc` を**直下だけ**数えたもの。`lisp/wip/` を
含めて再帰で数えると 136 になる。数え方が違うだけで、作られるものは同じ。

### CI でも同じものを回している

タグを打つと、`.github/workflows/release.yml` がビルドのあとバイトコンパイルと
テストを通す。**落ちれば公開へ進まない**ので、壊れたものが配られる経路は無い。

CI では **554/554 が期待値**。手元と違って OLE の 4 件も通る。ランナーでは
`InternetExplorer.Application` を作れるためで、落ちるのは画面を持たないデスクトップで
走らせたときだけ。それでも「落ちてよいのは OLE の 4 件だけ」という判定は残してある。
ランナーの世代が変われば落ちるようになりうる。

CI で xyzzy を起動する段には**必ず時間の上限を付ける**。`makelc` もテストも、失敗すると
`msgbox` を出して待つ。押せる人がいないランナーでは、上限が無いとジョブの制限時間まで
居座る。

### テストの前提

同梱の `run-tests.bat` は結果を `*Test Results*` バッファに出して**開いたままにする**
（人が読む前提）。自動で回すには、バッファをファイルへ書き出す Lisp を `-l` で読ませ、
最後に `-kill` を置く。`-kill` は `si:startup` の後に処理されるので、`-l` より後ろに
書けば処理が終わってから終了する。

`XYZZYCONFIGPATH` は **`wxp` という名前の空ディレクトリ**を指す。理由は 2 つ:

- 利用者の設定に触れない
- `user-config-path-and-dump-image-path-8-test` が `(pathname-name (user-config-path))`
  に `"wxp"` を期待している。別名を指すとこのテストだけ落ちる

設定を使い回すと、前回保存されたウィンドウ位置の `showCmd` が最小化 (2) のまま復元され、
`split-window` を使うテスト 3 件が「分割できません」で落ちる。**毎回空にする。**

**テストは cmd 経由で起動する。** `test-environ-1` / `-2` は `cmd.exe /c set` の出力と
`si:environ` を突き合わせる。cmd は起動時に自分の環境へ `PROMPT=$P$G` を足すので、
親の環境に `PROMPT` が無いと xyzzy 側にだけ存在しない変数ができ、この 2 件が落ちる。

**ドライブレターは大文字で渡し、カレントをリポジトリ直下にする。** `si:system-root` は
`GetModuleFileName` 由来で起動時の綴りを保つのに対し、`process-command-line` が基準に
する `(cwd)` は cmd 由来で大文字になる。小文字で起動すると `process-command-line--s` /
`--S` が `string=` で外れて落ちる。

## ビルド環境

- Visual Studio Build Tools 2022（IDE なし）、MSVC v143、Windows SDK 10
- 導入先は `vswhere` で探す。環境変数 `VS120COMNTOOLS` に依存していた `build.bat` と
  `misc/update_zlib.bat` は書き換え済み
- `projects/*.vcxproj` 9 本のうち `xyzzy.sln` が使う 5 本は `v143`。`projects/etc.sln`
  側の 4 本（`den8util` `extract` `jdwrap` `tail-f`）は `v120_xp` のままで、この
  ツールセットが無いとビルドできない

## ビルドの罠

### 増分ビルドは前回失敗したファイルを飛ばす

増分ビルドでは、**前回エラーで終わったファイルがそのまま飛ばされ、「0 エラー」と
表示される**ことがある。実際に、移行作業の途中で 0 エラーと出たのに `Rebuild` したら
61 箇所残っていた。**広く型を変えた後は必ず全部作り直す。**

```bash
misc/dev.sh build Release Rebuild
```

### 追加インクルードパス `/I src` を指定してはいけない

`cl` に `/I src` を付けると CRT の `<string.h>` が `src/string.h` に食われ、`lisp` 型が
未定義だという無関係なエラーが数百行出る。`src/` には `string.h` `array.h` `list.h` など
CRT / STL と名前が衝突するヘッダが多数ある。本来のビルドは `/I` を使わず、
`#include "foo.h"` が同一ディレクトリを先に見る挙動に頼っている。

### msbuild に .sln を渡すときは `/p:Platform=Win32` が要る

`/p:Configuration=Release` だけだと `error MSB4126` になる。

### `%(AdditionalOptions)` は継承値を先に置く

`%(AdditionalOptions)` は継承値に展開されるので、

```xml
<AdditionalOptions>/execution-charset:.932 %(AdditionalOptions)</AdditionalOptions>
```

と書くとプロジェクト全体の指定が後ろに来て、後勝ちで上書きされる。

```xml
<AdditionalOptions>%(AdditionalOptions) /execution-charset:.932</AdditionalOptions>
```

効いているかは、ビルドログの `CL.exe` の行で最後に来ている指定を見る。

### ファイル単位で `/execution-charset` を上書きしない

プロジェクト全体の指定と食い違うと警告 D9025 が出るうえ、PCH を作ったときの指定とも
ずれて `error C2855` になる。**プロジェクト単位で決める。**

### コンパイルを手で回すとき

複数ファイルを 1 回の `cl` 呼び出しに渡すと、どのファイルのエラーか判別できない。
ファイルごとにログへ落として個別に終了コードを見る。`cl` の出力は CP932 / CRLF なので
`iconv -f CP932 -t UTF-8` と `dos2unix` を通す。

### 実際に定義されるマクロは vcxproj を読んでも分からない

`UNICODE` / `_UNICODE` は `PreprocessorDefinitions` ではなく
`<CharacterSet>Unicode</CharacterSet>` から入る。MSBuild が `/D _UNICODE /D UNICODE`
へ展開するので、**vcxproj を grep しても綴りは出てこない**。

確実なのは建てたあとの記録を読むこと。UTF-16 なので `tr` でヌルを落とす。

```bash
tr -d '\0' < Release/xyzzy/xyzzy.tlog/CL.command.1.tlog | grep -o "/D [A-Za-z_0-9]*" | sort -u
```

## 文字符号

UTF-8（BOM なし）へ変換済み: `src/*.cc` `*.h`、`src/*.rc`、`*.bat`、`docs/old/*.html`、
`etc/README.gendic`、`lisp/*.l`、`unittest/*.l`、`misc/*.l`。

**Lisp のソースは読み手が符号を判別する。** `load` と `compile-file` は
`si:*detect-stream-encoding`（`src/stream.cc`）でファイル全体を厳密な UTF-8 として
検査し、通ったものだけを UTF-8 として読む。通らなければ CP932 として読むので、
利用者の `siteinit.l` や第三者の site-lisp が CP932 のままでも壊れない。
**新しく足す `.l` は UTF-8 で書く。** `open` の動作は変えていない。

**まだ CP932 のもの**:

- `etc/{IDL,TeX,Sql-NonStd/*}` — 言語ごとのキーワード表。`load-keyword-file` が
  `insert-file` で読む（`lisp/kwd.l`）ので、Lisp のソースとは別の経路になる
- **`.lc` は「正しい CP932」ではない。** バイトコードの命令語が `Char` 値として文字列
  リテラルに入るため、大半が CP932 として不正な後続バイトを含む。往復できているのは
  書き手と読み手がどちらも無検査だから。符号判別を導入するなら **`.l` だけに適用し
  `.lc` は素朴な連結器のままにする**非対称な設計が要る
- 変換に使う符号名は **`SHIFT_JIS` ではなく `CP932`**。波ダッシュ `0x8160` などで
  名前を誤ると写像がずれる
- **`misc/update-app.bat` は CP932。** 他の `.bat` と違って日本語を `echo` するため。
  cmd は `.bat` を CP932 として読むので、UTF-8 で書くと出力が化けるうえ、行が壊れる
  危険もある（`rem "..."` で括れるのはコメントだけで、`echo` の行は括れない）

**触らないもの**: `etc/DOC`（生成物）、`etc/dxflush.dxa`、`unittest/data/hello.txt.gz`
（バイナリ）、`src/dsfmt/README.jp.txt`（第三者コードの同梱物）

### 実行時の文字列は CP932 のまま

コンパイラに `/source-charset:utf-8 /execution-charset:.932` を指定しているので、
ソースは UTF-8 でも `char` の文字列リテラルは実行時 CP932 のバイト列になる。

| プロジェクト | execution-charset | 理由 |
|---|---|---|
| xyzzy / xyzzycli / xyzzyenv | `.932` | 実行時の文字列は CP932 |
| gen-src1 | `.932` | CP932 の変換表を作るのが仕事 |
| gen-src2 | `utf-8` | メッセージを UTF-8 で出力し、取り込む側が焼き直す |

`gen-syms` が `symtable.cc` へ書く文字列は 0x80 以上を 8 進エスケープするので、生成物は
純 ASCII。`gen-msg` が書く `msgdef.cc` は UTF-8。

### 編集の道具が CP932 のファイルを壊す

UTF-8 として読み書きする編集ツールで CP932 のファイルを触ると、**手を付けていない行の
日本語まで U+FFFD に潰れる**。見た目が変わらないので気付きにくい。`git diff` で
「見た目が同じ行が差分に出ている」ときはこれを疑う。**編集の前に符号化を確かめる。**

### `.bat` の日本語コメントは必ず `"` で括る

cmd.exe は `.bat` を OEM コードページ（日本語環境では 932）で読むため、**行末のバイトが
CP932 の先導バイトになると改行を食って行が壊れる**。

```
rem git describe の書式
                    ↑「式」の UTF-8 3 バイト目が 0x8F。CP932 では先導バイト → CR を食う
```

踏むと次の行の `rem` が外れて中身がコマンドとして実行される。実際に
`update_version_describe.bat` で git が暴発し exit 255 でビルドが止まった。

**対処**: `rem "日本語"` と括る。行末が必ず `"` (0x22) になり先導バイトになり得ない。

**効かない対処**: `chcp 65001`（cmd は先読みするので指定が効く前に読まれた行の被害が
残る）、BOM（cmd が BOM をコマンドの一部と解釈し `@echo off` 自体が失敗する）。

**不変条件**（`.bat` を編集したら必ず確認する）: 各行を行頭から CP932 として走査し、
行末で後続バイト待ちになっていないこと。「末尾が先導バイトか」だけを見るのは誤り。

```python
def lead(c): return 0x81 <= c <= 0x9f or 0xe0 <= c <= 0xfc
def eats(line: bytes) -> bool:
    i = 0
    while i < len(line):
        i += 2 if lead(line[i]) else 1
        if i > len(line): return True
    return False
```

### `.ps1` は ASCII だけで書く

`.bat` と同じ理屈。**Windows PowerShell 5.1 は BOM の無い `.ps1` を CP932 として読む**
ので、日本語の末尾バイトが先導バイトになると改行を食って次の行を巻き込む。実際に、
コメントの直後の代入が丸ごと消え、変数が `$null` のまま先へ進んだ。エラーは代入の行
ではなく数行あとで出るので原因が見えにくい。

**対処**: ASCII だけで書く。日本語を出すときは `[char]0x3042` のように符号位置から
組み立てる。`pwsh` (PowerShell 7) は UTF-8 として読むので踏まないが、`System.Drawing` を
`Add-Type` すると型の転送で失敗するため、画面まわりは `powershell.exe` になる。
つまり**画面まわりのスクリプトほどこの罠に当たる**。

## Lisp と `.lc`

### 書き換えたら検証の前にバイトコンパイルする

xyzzy は `lisp/*.l` ではなく `lisp/*.lc` を読む。`.l` を書き換えただけで起動しても
**古い `.lc` の定義がそのまま使われる**ので、変更が効かない。効いているかは
`grep -c <新しい呼び出し> lisp/foo.lc` で確かめられる。

### 組み込み関数を消す・改名するときは先に `.lc` を消す

`.lc` は読み込み時にも組み込み関数を呼ぶ（`lisp/charname.l` は load 時に
`make-character-name-hash-table` を走らせる）。組み込みを消したまま古い `.lc` を
読ませると、**起動直後に msgbox が出て無言で固まる**。バイトコンパイルしようにも
起動できないので、先に消しておくしかない。

```bash
rm -f lisp/*.lc lisp/wip/*.lc
misc/dev.sh bytecompile
```

**`.lc` はサブディレクトリにもある。** `lisp/*.lc` だけ消しても `lisp/wip/*.lc` が古い
まま残り、`makelc` の `compile-files` は `:recursive t` なのでそちらを読んで失敗する。
なお `lisp/wip/hellowin.lc` と `turtle.lc` は単体で読むと「パッケージが存在しません:
win-user」になる。これは元からで異常ではない。

### 古い `.lc` は起動そのものを妨げる

バイトコードの形式を変えた後、古い `.lc` が site-lisp に残っていると、**xyzzy は起動を
完了しない**（エラーダイアログが出る）。作り直すための xyzzy が起動できないので、
**先に `.lc` を退けてから**新しい xyzzy を起動し、`.l` から作り直す。
`misc/verify/recompile.l` がその作業をする。

### バイトコンパイルが無言で止まったら msgbox を疑う

`misc/makelc.l` の `makelc-and-exit` は、失敗すると `msgbox` を出してから終わる。
別デスクトップには押せる人がいないので、**エラーの内容が見えないまま永久に待つ**。
CPU をほとんど使わず `lisp/*.lc` の更新時刻も止まっているなら、これ。

内容を見るには `makelc-and-exit` ではなく `makelc` を直に呼び、条件をファイルへ落とす。

```lisp
(handler-case (makelc:makelc t)
  (condition (e) (…(format nil "~A" e)… をファイルへ)))
(kill-xyzzy nil)
```

driver から呼ぶときは `bytecompile.bat` の前処理も写すこと。**`lisp/startup.lc` を
消しておかないと**「既に存在するファイルを作成することはできません」で失敗する。

### 組み込み関数を足したら `builtin.l` にも宣言する

`src/gen-syms.cc` に `SI_DEFUN3` 等を足しただけだと `test-defun-builtin-1`
（builtin.l で定義されていない builtin 関数が存在しないこと）が落ちる。
`lisp/builtin.l` に `(si::defun-builtin ...)` を対で足す。

## 別デスクトップで検証するときの罠

### 例外を外へ逃がさない

別デスクトップではエラーダイアログが見えない。検証台の中で例外が外へ出ると、**誰にも
見えない場所でダイアログが出たまま止まり、時間切れになる**。すべて `ignore-errors` で
包むこと。

**読み込み時のエラーも同じ。** `si:` のような単一コロンで非エクスポートの名前を書いたり、
不正な書式指定を書いたりすると、`ignore-errors` の外側で読み手が signal するので、
**何も出力されないまま固まる**。原因が検証台の側にあると気付きにくい。

### `subprocess.STARTUPINFO` に `lpDesktop` は無い

別デスクトップで起動するつもりで次のように書いても、**Windows には何も届かず、利用者の
画面で起動する**。Python のオブジェクトに属性が生えるだけで、例外も警告も出ない。

```python
si = subprocess.STARTUPINFO()
si.lpDesktop = "xyzzy-offscreen"   # 無視される
subprocess.Popen([exe], startupinfo=si)
```

`subprocess.STARTUPINFO` が持つのは `dwFlags` `hStdInput` `hStdOutput` `hStdError`
`wShowWindow` `lpAttributeList` だけ。**対処**: `misc/verify/rundesk.py` を使う。
自分で書くなら `STARTUPINFOW` を `ctypes.Structure` で定義し `CreateProcessW` を直接
呼ぶ。窓が出ないことを当てにする仕掛けは、**外したときに誰も気づかないまま利用者の
画面を奪う**ので、`EnumDesktopWindows` で実際にそちらへ出ているか確かめてから使う。

`ctypes` の既定の `restype` は 32 ビット整数なので、`OpenDesktopW` や `CreateDesktopW`
の戻りをそのまま使うとハンドルが切り詰められ、`EnumDesktopWindows` が何も返さない。
`restype` と `argtypes` を明示する。

なお `GetGUIThreadInfo` は**呼ぶ側のスレッドが同じデスクトップに居ないと**
`ERROR_ACCESS_DENIED` になる。`SetThreadDesktop` で移ってから訊く。

### ウィンドウを撮る手は 2 通りあり、使い分けが要る

**画面から切り出す (`CopyFromScreen`)**: どんなウィンドウでも撮れるが、**前面に出す必要が
あり、上に乗った別のウィンドウごと写る**。しかも `SetForegroundWindow` はバックグラウンド
のプロセスからでは弾かれることが多い。

**`PrintWindow`**: 前面に出さなくてよく他の窓も写らないが、対象が `WM_PRINTCLIENT` に
応じる必要がある。**xyzzy の本体ウィンドウは応じないので白紙になる**が、共通設定の
ような標準のダイアログ (`#32770`) は応じるので撮れる。

**どちらでも、撮る側のプロセスを DPI 認識にしておく。** 非認識だと `GetWindowRect` が
仮想化された座標を返す。`CopyFromScreen` は縮小された絵が撮れるだけだが、
**`PrintWindow` は実寸で描くので左上の一部しか写らない**。

撮る前に対象が本当に目的のウィンドウか確かめること。前面化に失敗したまま画面座標を
コピーして他人のウィンドウを写した事故がある。

## その他

### ダンプイメージは exe を替えたら消す

`xyzzy.wxp` は起動を速くするためのダンプ。読み込むかどうかは `dump_version` で決まるが、
その値は `src/gen/dumpver.cc` が**生成された時刻**であって exe をリンクした時刻ではない。
**exe を差し替えただけでは古いダンプが「有効」と判定される。** 実際に、版番号を変えた
のに題が古いままになった。exe を入れ替えるときは `xyzzy.wxp` を消す。

### ヒアドキュメントはバックスラッシュを潰す

シェルのヒアドキュメントは `<<'EOF'` と引用符付きで書いても `\\` が `\` に縮むことが
ある。バックスラッシュを含むファイル（Windows パスを組み立てるスクリプト等）は
ヒアドキュメントで作らない。

### `VAR= cmd` と `set VAR=` は違う

シェルの前者は**空文字列をセット**し、cmd の後者は**変数を消す**。`XYZZYINIFILE=` を
空でセットすると `getenv` が空文字列を返して ini のパス解決が狂い、子プロセスにも
伝染して `user-config-path-*` 系が 3 件落ちる。消したいなら `env -u VAR`。

### `git merge -F -` は標準入力を読まない

`git commit -F -` はヒアドキュメントを受け取るが、**`git merge -F -` は `-` をファイル名と
解釈して `could not read file '-'` で落ちる**。しかも `git switch` は先に済んでいるので、
ブランチだけ切り替わってマージされていない状態が残る。メッセージは一旦ファイルへ書く。
