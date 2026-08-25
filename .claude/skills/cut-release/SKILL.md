---
name: cut-release
description: xyzzy の版を上げてリリースを刻む。「リリースして」「版を上げて」「タグを打って」等で起動する。版番号の更新・タグ・ビルド・題の確認までを順に行なう。タグを打たずにビルドすると題に git のハッシュと dirty が出るので、順序が肝心。
---

# リリースを刻む

版番号を上げ、タグを打ち、そのタグの上でビルドする。**順序を守らないと題に
`v0.2.2.253-126-g81765f8-dirty` のような文字列が出る。**

## なぜ順序が要るか

題（タイトルバー）に出るのは `DisplayVersionString`。`misc/update_version_describe.bat`
が `git describe --tags --dirty` の結果を `src/gen/version-describe.h` に書き、
`src/version.cc` がそれを採る。ただし **`describe` と `describe --long` が食い違うとき
＝ちょうどタグの上に居るとき**だけ「リリース」と判定して describe を空にし、
題は版番号そのもの（`PROGRAM_VERSION`）に落ちる。

つまり**タグの無いコミットで建てると、必ずハッシュが題に出る**。

## 手順

### 1. 版番号を決めて上げる

`src/version.h` の 5 つのマクロ。本家からの派生を表すなら
`PROGRAM_PATCH_LEVEL` を使うと 5 つ目の要素になる。

```c
# define PROGRAM_MAJOR_VERSION 0
# define PROGRAM_MINOR_VERSION 2
# define PROGRAM_MAJOR_REVISION 2
# define PROGRAM_MINOR_REVISION 253
# define PROGRAM_PATCH_LEVEL 1      // → 0.2.2.253.1
```

**Windows のファイルバージョンは 16 ビット整数 4 つ**と決まっているので、資源に載る
`FILEVERSION` は前の 4 つだけ。5 つ目は表示と `si:software-version` にしか出ない。

`lisp/estartup.l` の `integer-version` はちょうど 5 要素を見るので、5 つ目まで正しく
比較される。

### 2. コミットしてからタグを打つ

**作業ツリーがきれいであること**を確かめる。汚れたまま建てると `-dirty` が付く。

```bash
git status --short          # 空であること
git commit ...              # 版番号の変更を入れる
git tag v0.2.2.253.1        # 既存のタグと同じ綴りで
```

打った後に確かめる。**二つが食い違えばリリース扱いになる。**

```bash
git describe --tags --dirty          # v0.2.2.253.1
git describe --tags --dirty --long   # v0.2.2.253.1-0-gXXXXXXX
```

### 3. 建てる

```bash
misc/dev.sh build Release
```

### 4. 題を確かめる

exe に埋まる文字列は UTF-16 なので、ASCII の grep では出ない。

```bash
MSYS_NO_PATHCONV=1 python -c "
import pathlib
d = pathlib.Path(r'<リポジトリ>\xyzzy.exe').read_bytes()
def has(s): return 'YES' if s.encode('utf-16-le') in d else 'no'
print('dirty      :', has('dirty'))
print('xyzzy 0.2.2.253.1 :', has('xyzzy 0.2.2.253.1'))
"
```

`dirty` が `no`、版番号入りの題が `YES` になっていればよい。

### 5. 通しで確かめる

```bash
misc/dev.sh bytecompile
misc/dev.sh test
```

期待値は `docs/development.md` を見る。

## 忘れやすいこと

- **ダンプイメージ `xyzzy.wxp` は exe を替えても無効にならない。** 判定に使う
  `dump_version` は `src/gen/dumpver.cc` が生成された時刻であって、リンクした時刻では
  ない。**古いダンプが残っていると題も古いまま**なので、確認の前に消す
- リリースノートを書くなら `docs/release-note-<版>.md`。既存の並びに合わせる
- タグを打った後で版番号を直したくなったら、タグも打ち直す（`git tag -d`）
