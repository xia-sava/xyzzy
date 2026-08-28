---
name: xyzzy-lisp
description: xyzzy の Lisp API を引く。xyzzy の .l を書く・読む・直すとき、組み込み関数のシグネチャや挙動を確かめるとき、メジャーモード・キーマップ・シンタックステーブル・正規表現による色付けを定義するとき、reference/reference.xml に項目を足すときに使う。
---

# xyzzy Lisp

xyzzy の拡張言語は Common Lisp の部分集合にエディタ API を足したもので、
ANSI CL でも Emacs Lisp でもない。**推測で関数名を書かないこと。**
名前が近いだけの別物や、そもそも無いものが多い。

索引と本文は `.claude/skills/xyzzy-lisp/references/` にある。以下のパスは
リポジトリ直下から書いてある。全文は読まず、必要な分だけ引く。

## 引き方

**1. 索引で存在とシグネチャを確かめる。** `index.tsv` は
`name / type / package / section / arguments` のタブ区切り。

```bash
R=.claude/skills/xyzzy-lisp/references
grep -P '^set-text-attribute\t' $R/index.tsv
grep -P '^re-search' $R/index.tsv                  # 前方一致
grep -P '\tbuffers\t' $R/index.tsv | cut -f1,5     # 節で絞る
```

**2. 本文は節の md から見出しだけ抜く。** 索引の 4 列目が節の名前。

```bash
awk '/^## `set-text-attribute`$/{f=1;next} f&&/^## /{exit} f' \
    .claude/skills/xyzzy-lisp/references/text.md
```

**3. 索引に無くても「無い」とは限らない。** 索引は
`reference/reference.xml` にある**文書化済みの API** で、実装の全部ではない。

| | |
|---|---|
| 索引の項目 | 1856 |
| `src/gen-syms.cc` の組み込み | 1034（うち 180 は索引に無い） |
| `lisp/*.l` のトップレベル定義 | 2968 |

索引に無ければ、この順で当たる。

```bash
grep -nE '^ *(DEFUN3|DEFCMD3) *\(名前,' src/gen-syms.cc
grep -rnE '^\(def(un|macro|var|parameter) +名前( |\))' lisp/
```

名前の終わりは空白か閉じ括弧で止める。`\b` はハイフンの手前でも境界になるので、
`save-buffer` が `save-buffer-dialog` に当たってしまう。

そこにも無ければ本当に無い。CL や Emacs Lisp にあることを根拠に使わない。

**4. 実装を読むなら** 索引の節 md にある「定義:」を見る。`builtin.l` と
書いてあるものは `lisp/builtin.l` の `si::defun-builtin` が**宣言だけ**しており、
実体は `src/` の C++ にある。

## 節の一覧

```
datatypes variables control-flow packages functions macros symbols numbers
characters strings sequences lists arrays hashtables evaluation errors io
filesystem processes system datetime buffers windows frames positions regions
text search-regexp syntax keymaps modes minibuffer menus dialogs filer chunks
misc
```

## パッケージと種別

| パッケージ | 中身 | 数 |
|---|---|---|
| `editor` | エディタ API（バッファ、ウィンドウ、キーマップ…） | 1137 |
| `lisp` | Common Lisp 由来 | 635 |
| `system`（`si`） | 内部・低水準 | 47 |
| `keyword` | キーワード | 9 |

拡張は `(in-package "editor")` の中に書く。

種別は `Function` 1475 / `Variable` 223 / `Macro` 68 / `Special Form` 26 /
`Tips` 24 / `BufferLocal` 18 / `Misc` 10 / `Keyword` 9 / `Accessor` 2 /
`Struct` 1。`Tips` は関数ではなく解説の項で、「正規表現の表記」のように
まとまった説明がここに入っている。

## Common Lisp との差

- **CLOS が無い。** `defclass` `defgeneric` `defmethod` `make-instance` は
  どれも存在しない。構造体は `defstruct` を使う。
- **スレッドが無い。** 非同期は `make-process` に
  `set-process-filter` / `set-process-sentinel` を付けるか、`start-timer` を
  使うかの二通り。
- 数値・シーケンス系はおおむね CL に沿うが、欠けているものも多い。索引で確かめる。
- **JSON の読み書きは無い。** ソケットは `connect` があり、
  `ssl-do-handshake` / `socket-stream-ssl-p` で TLS も扱える。

## Emacs Lisp との差

- **正規表現はバックスラッシュ記法。** グループは `\\(...\\)`、
  繰り返しは `\\{4\\}`、単語境界は `\\b`。`(` `|` はそのままだと文字そのもの。
  PCRE ではない。詳しくは索引の `正規表現の表記`。
- **overlay が無い。** 見た目を変えるのは `set-text-attribute`（タグを付ける）で、
  `delete-text-attributes` / `clear-all-text-attributes` で消す。
- バッファに無い文字を画面へ描く手段が無い。
- 名前が微妙に違うものが多い。索引を引かずに書かない。

## この私家版で変わっているところ

上流の 0.2.2.235 から意図して変えた点は
`docs/incompatible-changes.md` にある。Lisp を書くうえで効くのはこの二つ。

- **文字は Unicode の符号位置。** `(char-code #\あ)` は `#x82A0` ではなく
  `#x3042`。数値の範囲で文字種を判定しているコードは書き換えが要る。
- **キーを表す文字は Unicode の外。** `(char-code #\F1)` は `#x20000F`、
  `(char-code #\M-a)` は `#x800061`。`#x8000` `#xFE00` `#xFF00` の区画には
  もう無い。Shift と Ctrl が下位バイトの `#x40` `#x80` なのは変わらない。

## 索引と本文の作り直し

`references/` は `reference/reference.xml` から作った生成物で、直接直しても
次の生成で消える。項目を足す・直すときは原稿の方を直して作り直す。

```bash
python misc/gen-reference-skill.py
python misc/gen-reference-skill.py --check   # 古ければ exit 1
```
