---
name: cut-release
description: xyzzy の版を上げてリリースを刻む。「リリースして」「版を上げて」「タグを打って」等で起動する。版番号の更新・変更点の記載・タグ・公開・確認までを順に行なう。建てるのは CI なので、タグと src/version.h が食い違うと組む前に落ちる。
---

# リリースを刻む

版番号を上げ、タグを打って push する。**建てて公開するのは CI**
（`.github/workflows/release.yml`）なので、手元で建てたものは配布物にならない。

## 何が自動で、何が手作業か

| | |
|---|---|
| 版番号を上げる | 手 |
| 変更点を書く | 手 |
| コミット・タグ | 手 |
| push | 手（**外に出る操作なので毎回確認を取る**） |
| ビルド・書庫作り・公開 | CI |

CI はタグと `src/version.h` の一致を確かめ、**食い違えば組む前に落とす**。題に
ハッシュが出ないことも CI が担保する（タグの上で建てるため）。

## 手順

### 1. 版番号を決めて上げる

`src/version.h` の 5 つのマクロ。本家からの派生を表すなら
`PROGRAM_PATCH_LEVEL` を使うと 5 つ目の要素になる。

```c
# define PROGRAM_MAJOR_VERSION 0
# define PROGRAM_MINOR_VERSION 2
# define PROGRAM_MAJOR_REVISION 2
# define PROGRAM_MINOR_REVISION 253
# define PROGRAM_PATCH_LEVEL 2      // → 0.2.2.253.2
```

**Windows のファイルバージョンは 16 ビット整数 4 つ**と決まっているので、資源に載る
`FILEVERSION` は前の 4 つだけ。5 つ目は表示と `si:software-version` にしか出ない。
そのため**実行ファイルから 5 つ目は読めない**。更新機構が導入先に版を記録するのは
このため。

`lisp/estartup.l` の `integer-version` はちょうど 5 要素を見るので、5 つ目まで正しく
比較される。

### 2. 変更点を `docs/RELEASE.md` に書く

派生してからの変更点を版ごとにまとめたもの。README からリンクしており、`docs/` は
`archive.bat` がまるごと配布物へ入れるので、**解凍した利用者もこれを読む**。

**コミット単位ではなく主題単位でまとめる。** 利用者が知りたいのは何が変わったかで
あって、どういう手順で作業したかではない。`git log` をそのまま並べない。

  * 新しい版を一番上に足す。見出しは `## <版> (<日付>)`
  * 直した不具合には、**どう見えていたか**を一行添える。利用者は症状で覚えている
  * Lisp から見える非互換は `docs/incompatible-changes.md` に移行方法まで書き、
    こちらからは触れるだけにする

`docs/release-note-0.2.2.2xx.md` は派生前の各版の記録なので、増やさない。

### 3. 手元で通しておく

公開してから落ちると取り返しがつかないので、先に通す。

```bash
misc/dev.sh build Release Rebuild
misc/dev.sh bytecompile
misc/dev.sh test
```

期待値は `docs/development.md` を見る。

### 4. コミットしてタグを打つ

**版上げのコミットは `src/version.h` だけにする。** 過去の版がそうなっているし、
タグの載る commit に他のものを混ぜない。変更点は先に別のコミットにする。

```bash
git status --short          # 他に未コミットのものが無いこと
git commit -m '派生してからの変更点を RELEASE.md にまとめる'   # docs/RELEASE.md
git commit -m 'バージョンを 0.2.2.253.2 にする'                # src/version.h だけ
git tag v0.2.2.253.2        # 既存のタグと同じ綴りで
```

打った後に確かめる。**二つが食い違えばリリース扱い**になり、題は版番号そのものに
なる。

```bash
git describe --tags --dirty          # v0.2.2.253.2
git describe --tags --dirty --long   # v0.2.2.253.2-0-gXXXXXXX
```

### 5. push する

**ここから外に出る。実行する直前に必ず確認を取ること。**

```bash
git push origin develop
git push origin v0.2.2.253.2   # ← これで CI が動く
```

ブランチだけの push では CI は動かない。動くのはタグの push と手動実行だけ。

### 6. CI を見届ける

```bash
gh run list --repo xia-sava/xyzzy --limit 3
gh run watch <run-id> --repo xia-sava/xyzzy --exit-status
```

`build` と `release` の 2 つが通ればよい。`build` はタグと版の一致・0 警告 0 エラー・
書庫の中身（派生物が入っていないこと）まで見る。

### 7. 公開されたものを確かめる

```bash
gh release view latest --repo xia-sava/xyzzy --json assets -q '.assets[].name'
curl -fsSL https://github.com/xia-sava/xyzzy/releases/download/latest/manifest.json
```

`latest` に `xyzzy.zip` と `manifest.json`、版ごとのタグに `xyzzy-<版>.zip` が
載っていること。`manifest.json` の `version` が打ったタグと一致すること。

## 配ったあと

導入済みの xyzzy は **[ヘルプ]-[更新を確認]** から自分で取りに行ける。更新機構を
まだ持っていない導入先には、一度だけ手で当てる。

```
misc\update-app.bat <導入先>
```

## 忘れやすいこと

- **`latest` は git のタグとしても作られる。** `git fetch` で降りてくるので、手元で
  `git describe` の結果が変わることがある
- **変更点は `docs/RELEASE.md` に足す。** 版ごとのファイルは作らない
- タグを打った後で内容を直したくなったら、タグを消してコミットごと解き直す
  （`git tag -d` して `git reset --mixed <直前のコミット>`）。push 前なら手元で
  済むが、**push した後ならリモートのタグも消す必要がある**
- **ダンプイメージ `xyzzy.wxp` は exe を替えても無効にならない。** 判定に使う
  `dump_version` は `src/gen/dumpver.cc` が生成された時刻であって、リンクした時刻では
  ない。手元で題を確かめるときは先に消す
