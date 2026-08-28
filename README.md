# xyzzy

xyzzy は[亀井哲弥氏](http://www.jsdlab.co.jp/~kamei/) が開発した、Common Lisp っぽい言語で拡張可能な
Emacs っぽいテキストエディタのようなものです。
2ch とか Twitter とか五目並べができたり、テキストファイルの読み書きができます。

亀井氏のあとは有志が [xyzzy-022](https://github.com/xyzzy-022/xyzzy) で開発を継続し、
0.2.2.253 まで出ました。


----

## このリポジトリについて

**ここは私家版です。公式でも、有志による本流でもありません。**
自分が毎日使うために手を入れたものを、そのまま置いてあります。

```
亀井哲弥氏のオリジナル (0.2.2.235)
 └ xyzzy-022/xyzzy    有志による継続開発 (0.2.2.253)
    └ xia-sava/xyzzy  ← ここ (0.2.2.253.x)
```

  * **無保証です。** 手元の Windows 11 で動くことしか確かめていません。
  * **不具合の報告先はここです。** この版で起きたことを xyzzy-022 に持ち込むと
    向こうが困ります。[Issues](https://github.com/xia-sava/xyzzy/issues) へどうぞ。
  * 手を入れていない xyzzy が欲しい場合は
    [xyzzy-022/xyzzy](https://github.com/xyzzy-022/xyzzy) を見てください。

Lisp の動作は 0.2.2.235 と互換であることを原則にしています。現代化のために
やむを得ず破ったものは、内容と移行方法を
[docs/incompatible-changes.md](docs/incompatible-changes.md) にまとめてあります。
とくに**文字を Unicode の符号位置で持つ**ようにしたので、文字を数値で扱う Lisp は
影響を受けます。


----

## 開発

私家版なので、開発は `develop` ブランチ一本で進めます。

ビルドと検証の手順は [docs/development.md](docs/development.md) にあります。

  * コミットログの書き方
    * Git 流 (メールみたいなやつ) に従うが日本語で良い
    * エンコーディングは UTF-8 で
  * コーディングルール
    * 元々の亀井さんのコードに合わせます
      * misc/mode-settings.l を読み込んでおいてください
  * .gitconfig に以下を設定しておく

    ```ini
    [i18n]
            commitencoding = utf-8
            logoutputencoding = utf-8
    [gui]
            encoding = utf-8
    ```


----

## リリース

  * 版番号は 0.2.2.253 の後ろにもう一桁足した `0.2.2.253.x` で、
    リリースのたびに末尾を 1 ずつ増やします
  * タグを打つと CI が配布物を組んで公開します
  * 派生してからの変更点は [docs/RELEASE.md](docs/RELEASE.md) にまとめています。
    Lisp から見える動作の非互換は
    [docs/incompatible-changes.md](docs/incompatible-changes.md) に移行方法とともに
    記載しています


----

## サポート OS

  * Windows 10 / 11 をサポート


----

## ビルド方法

Visual Studio Build Tools 2022 が要ります。

```bash
misc/dev.sh all      # ビルド → バイトコンパイル → テスト
```

バイトコンパイルとテストは xyzzy を起動しますが、別デスクトップで走るので画面には
出ません。道具立ての詳細、踏みやすい罠、CI との関係は
[docs/development.md](docs/development.md) に書いてあります。


----

## ライセンス

MIT ライセンスです。
詳細は LICENSE ファイルを参照のこと。
