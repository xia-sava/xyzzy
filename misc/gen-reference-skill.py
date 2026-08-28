#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""reference/reference.xml から、Lisp API を引くための参照ファイルを作る。

出力先は .claude/skills/xyzzy-lisp/references/:

  index.tsv    全エントリの索引。名前・種別・パッケージ・節・引数のタブ区切り
  <slug>.md    節ごとの本文

  python misc/gen-reference-skill.py
  python misc/gen-reference-skill.py --check   # 古ければ exit 1
"""

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE = REPO_ROOT / "reference" / "reference.xml"
OUTDIR = REPO_ROOT / ".claude" / "skills" / "xyzzy-lisp" / "references"

# <section> は日本語なので、ファイル名に使える綴りへ対応づける。
# 節が増えたらここに足す。載っていない節があれば生成時に止まる。
SECTION_SLUGS = {
    "データ型": "datatypes",
    "変数と定数": "variables",
    "制御構造": "control-flow",
    "パッケージ": "packages",
    "関数": "functions",
    "マクロ": "macros",
    "シンボル": "symbols",
    "数値": "numbers",
    "文字": "characters",
    "文字列": "strings",
    "シーケンス": "sequences",
    "リスト": "lists",
    "配列": "arrays",
    "ハッシュ": "hashtables",
    "評価": "evaluation",
    "エラー": "errors",
    "入出力": "io",
    "ファイルシステム": "filesystem",
    "プロセス": "processes",
    "システム": "system",
    "日付・時間": "datetime",
    "バッファ": "buffers",
    "ウィンドウ": "windows",
    "フレーム": "frames",
    "ポジション": "positions",
    "リージョン": "regions",
    "テキスト": "text",
    "検索・正規表現": "search-regexp",
    "シンタックス": "syntax",
    "キーマップ": "keymaps",
    "モード": "modes",
    "ミニバッファ": "minibuffer",
    "メニュー": "menus",
    "ダイアログ": "dialogs",
    "ファイラ": "filer",
    "チャンク": "chunks",
    "その他": "misc",
}

# 原稿の <package> に入っている、パッケージ名でない綴り。
UNKNOWN_PACKAGE = {"???"}


def text_of(chapter, tag):
    node = chapter.find(tag)
    return node.text.strip() if node is not None and node.text else ""


def texts_of(chapter, tag):
    return [n.text.strip() for n in chapter.findall(tag) if n.text and n.text.strip()]


def one_line(s):
    """索引はタブ区切りなので、空白を潰して 1 行にする。"""
    return re.sub(r"\s+", " ", s).strip()


def parse():
    entries = []
    unknown = set()
    for ch in ET.parse(SOURCE).getroot().findall("chapter"):
        section = text_of(ch, "section")
        if section not in SECTION_SLUGS:
            unknown.add(section)
            continue
        package = text_of(ch, "package")
        entries.append(
            {
                "name": text_of(ch, "title"),
                "type": text_of(ch, "type"),
                "arguments": text_of(ch, "arguments"),
                "package": "" if package in UNKNOWN_PACKAGE else package,
                "description": text_of(ch, "description"),
                "refs": texts_of(ch, "seealso") + texts_of(ch, "link"),
                "section": section,
                "slug": SECTION_SLUGS[section],
                "file": text_of(ch, "file"),
            }
        )
    if unknown:
        sys.exit(
            "SECTION_SLUGS に無い <section> があります: " + ", ".join(sorted(unknown))
        )
    return entries


def in_order(entries):
    return sorted(entries, key=lambda e: (e["name"].lower(), e["name"]))


def render_index(entries):
    lines = ["# name\ttype\tpackage\tsection\targuments"]
    for e in in_order(entries):
        lines.append(
            "\t".join(
                [
                    e["name"],
                    e["type"] or "-",
                    e["package"] or "-",
                    e["slug"],
                    one_line(e["arguments"]) or "-",
                ]
            )
        )
    return "\n".join(lines) + "\n"


def render_section(section, slug, entries):
    out = [
        f"# {section} ({slug})",
        "",
        f"reference/reference.xml から作った {len(entries)} エントリ。",
        "ここを直しても次の生成で消える。直すなら reference/reference.xml を直す。",
        "",
    ]
    for e in in_order(entries):
        out += [f"## `{e['name']}`", ""]
        meta = [e["type"] or "?"]
        if e["package"]:
            meta.append(f"package: {e['package']}")
        if e["file"]:
            meta.append(f"定義: {e['file']}")
        out.append("- " + " / ".join(meta))
        if e["arguments"]:
            out.append(f"- 呼び出し: `{one_line(e['arguments'])}`")
        out.append("")
        if e["description"]:
            out += ["```text", e["description"], "```", ""]
        if e["refs"]:
            out += ["関連: " + ", ".join(f"`{r}`" for r in e["refs"]), ""]
    return "\n".join(out)


def build(entries):
    by_section = {}
    for e in entries:
        by_section.setdefault((e["section"], e["slug"]), []).append(e)
    files = {"index.tsv": render_index(entries)}
    for (section, slug), es in by_section.items():
        files[f"{slug}.md"] = render_section(section, slug, es)
    return files


def read(path):
    """書き出しは LF なので、読むときも改行を変換させない。"""
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write(path, body):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(body)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="古いかどうかだけ見る")
    args = ap.parse_args()

    entries = parse()
    files = build(entries)
    summary = f"{len(entries)} entries / {len(files) - 1} sections"

    if args.check:
        stale = sorted(
            name
            for name, body in files.items()
            if not (OUTDIR / name).is_file() or read(OUTDIR / name) != body
        )
        extra = sorted(
            p.name for p in OUTDIR.iterdir() if p.is_file() and p.name not in files
        ) if OUTDIR.is_dir() else []
        if stale or extra:
            sys.exit(
                "generated files are out of date; run: python misc/gen-reference-skill.py\n"
                + (f"  stale: {', '.join(stale)}\n" if stale else "")
                + (f"  extra: {', '.join(extra)}\n" if extra else "")
            )
        print(f"up to date ({summary})")
        return

    OUTDIR.mkdir(parents=True, exist_ok=True)
    for old in OUTDIR.iterdir():
        if old.is_file() and old.name not in files:
            old.unlink()
    for name, body in files.items():
        write(OUTDIR / name, body)
    print(f"wrote {OUTDIR.relative_to(REPO_ROOT).as_posix()} ({summary})")


if __name__ == "__main__":
    main()
