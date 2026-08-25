#!/usr/bin/env bash
# 配布されている最新版を取ってきて、導入済みの xyzzy を置き換える。
#
#   misc/update-app.sh [選択肢] <導入先>
#
#     --dry-run          何をするかだけ表示し、一切変更しない
#     --force            同じ版でも適用する
#     --replace-backup   退避先が既にあるとき、それを捨てて上書きする
#
#   <導入先> は環境変数 XYZZY_INSTALL_DIR でも渡せる。
#
# 置き換えるのは実行ファイルと lisp・etc だけで、利用者のものには触らない。
# docs と reference も触らない（導入先の方が新しい形式を持っていることがある）。
#
# 順序に制約がある。古い .lc が site-lisp に残っていると、バイトコードの形式が
# 変わった新しい xyzzy は起動を完了しない。作り直すための xyzzy が起動できなく
# なるので、置き換えと同時に .lc を退ける。.lc が無ければ .l が読まれるので
# (src/pathname.cc の探索順)、退けるだけで動く。作り直しは後から xyzzy の中でやる。
#
# 同じ理由でダンプも消す。読み込むかどうかを決める dump_version は生成された時刻
# であって exe をリンクした時刻ではないため、置き換えただけでは古いダンプが
# 「有効」と判定される。

set -u

# 配布物の置き場。試すときは file:// を指す場所へ向けられる。
BASE_URL="${XYZZY_UPDATE_BASE_URL:-https://github.com/xia-sava/xyzzy/releases/download/latest}"

# 適用した版を導入先に記録する。実行ファイルの資源は版番号を 4 つしか持たない
# (src/version-rc.h) ため、0.2.2.253 と 0.2.2.253.1 を exe からは区別できない。
STAMP_NAME=".xyzzy-update"

DRY_RUN=0
FORCE=0
REPLACE_BACKUP=0
INSTALL_DIR=""

# 退避を終えてから転ぶと、導入先が中途半端なまま残る。戻し方を必ず添える。
BACKUP_DONE=0
die () {
  printf 'update-app: %s\n' "$*" >&2
  if [ "$BACKUP_DONE" -eq 1 ]; then
    printf '\n導入先が中途半端になっている。退避から戻すこと:\n  rm -rf "%s" && mv "%s" "%s"\n' \
      "$INSTALL_DIR" "$BACKUP_DIR" "$INSTALL_DIR" >&2
  fi
  exit 1
}
say () { printf '%s\n' "$*"; }
act () { if [ "$DRY_RUN" -eq 1 ]; then printf '        (試行) %s\n' "$*"; else printf '        %s\n' "$*"; fi; }

usage () {
  sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'
  exit 1
}

while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY_RUN=1 ;;
    --force) FORCE=1 ;;
    --replace-backup) REPLACE_BACKUP=1 ;;
    -h|--help) usage ;;
    -*) die "知らない選択肢: $1" ;;
    *) [ -n "$INSTALL_DIR" ] && die "導入先を 2 つ受け取った: $INSTALL_DIR と $1"
       INSTALL_DIR="$1" ;;
  esac
  shift
done

[ -n "$INSTALL_DIR" ] || INSTALL_DIR="${XYZZY_INSTALL_DIR:-}"
[ -n "$INSTALL_DIR" ] || usage

# /c/foo -> C:/foo （ドライブレターは大文字へ）。PowerShell へ渡すために要る。
to_win () {
  local p="$1"
  if [[ "$p" =~ ^/([a-zA-Z])/(.*)$ ]]; then
    printf '%s:/%s' "$(printf '%s' "${BASH_REMATCH[1]}" | tr '[:lower:]' '[:upper:]')" "${BASH_REMATCH[2]}"
  else
    printf '%s' "$p"
  fi
}

# 平らな JSON から文字列の値を取り出す。読む相手は自分たちが書いた manifest.json
# だけなので、形が決まっていることに頼ってよい。
json_value () {
  sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" "$1" | head -1
}

for tool in curl 7z sha256sum; do
  command -v "$tool" >/dev/null 2>&1 || die "$tool が見つからない"
done

[ -d "$INSTALL_DIR" ] || die "導入先が無い: $INSTALL_DIR"
[ -f "$INSTALL_DIR/xyzzy.exe" ] || die "xyzzy.exe が無いので導入先とは思えない: $INSTALL_DIR"

INSTALL_DIR="$(cd "$INSTALL_DIR" && pwd)"

# 動作中かどうかは PowerShell に訊くので、Windows のパスが要る。bash から見える
# 名前は /tmp のような別名になっていることがあり、そのまま渡すと何にも一致せず、
# 「動いていない」と黙って通ってしまう。直せなければ止める。
if command -v cygpath >/dev/null 2>&1; then
  INSTALL_WIN="$(cygpath -m "$INSTALL_DIR")"
else
  INSTALL_WIN="$(to_win "$INSTALL_DIR")"
fi
[[ "$INSTALL_WIN" =~ ^[A-Za-z]:/ ]] \
  || die "導入先を Windows のパスへ直せない: $INSTALL_DIR -> $INSTALL_WIN"
BACKUP_DIR="$INSTALL_DIR.bak"
LC_HOLD_DIR="$INSTALL_DIR.lc-hold"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

say "導入先        $INSTALL_DIR"
say "配布物の置き場 $BASE_URL"
say ""

# --- 1. 配布中の版を調べる ------------------------------------------------
say "[1/7] 配布中の版を調べる"
curl -fsSL "$BASE_URL/manifest.json" -o "$WORK/manifest.json"
[ $? -eq 0 ] || die "manifest.json を取得できない"

NEW_VERSION="$(json_value "$WORK/manifest.json" version)"
NEW_ASSET="$(json_value "$WORK/manifest.json" asset)"
NEW_SHA="$(json_value "$WORK/manifest.json" sha256)"
NEW_COMMIT="$(json_value "$WORK/manifest.json" commit)"
[ -n "$NEW_VERSION" ] && [ -n "$NEW_ASSET" ] && [ -n "$NEW_SHA" ] \
  || die "manifest.json を読めない"

STAMP="$INSTALL_DIR/$STAMP_NAME"
if [ -f "$STAMP" ]; then
  OLD_VERSION="$(sed -n 's/^version=//p' "$STAMP" | head -1)"
  OLD_SHA="$(sed -n 's/^sha256=//p' "$STAMP" | head -1)"
else
  OLD_VERSION=""
  OLD_SHA=""
fi

if [ -n "$OLD_VERSION" ]; then
  say "        導入済み  $OLD_VERSION"
else
  say "        導入済み  不明（このスクリプトで更新した記録が無い）"
fi
say "        配布中    $NEW_VERSION (commit ${NEW_COMMIT:0:9})"

if [ -n "$OLD_SHA" ] && [ "$OLD_SHA" = "$NEW_SHA" ] && [ "$FORCE" -eq 0 ]; then
  say ""
  say "配布中のものが既に入っている。--force で強制できる。"
  exit 0
fi

# --- 2. 落として照合する --------------------------------------------------
say "[2/7] 配布物を落として照合する"
curl -fsSL "$BASE_URL/$NEW_ASSET" -o "$WORK/$NEW_ASSET"
[ $? -eq 0 ] || die "$NEW_ASSET を取得できない"

GOT_SHA="$(sha256sum "$WORK/$NEW_ASSET" | cut -d' ' -f1)"
if [ "$GOT_SHA" != "$NEW_SHA" ]; then
  die "照合に失敗した（sha256 が合わない）
  期待 $NEW_SHA
  実際 $GOT_SHA"
fi
say "        sha256 一致"

7z x -o"$WORK/x" "$WORK/$NEW_ASSET" > /dev/null 2>&1
[ $? -eq 0 ] || die "配布物を展開できない"

SRC="$WORK/x/xyzzy"
for f in xyzzy.exe xyzzycli.exe xyzzyenv.exe lisp etc; do
  [ -e "$SRC/$f" ] || die "配布物に $f が無い"
done
say "        展開した"

# --- 3. 動いていないことを確かめる ----------------------------------------
say "[3/7] 動いていないことを確かめる"
RUNNING="$(powershell.exe -NoProfile -NonInteractive -Command \
  "Get-CimInstance Win32_Process -Filter \"Name='xyzzy.exe' OR Name='xyzzycli.exe' OR Name='xyzzyenv.exe'\" |
   Where-Object { \$_.ExecutablePath -and \$_.ExecutablePath.StartsWith('$INSTALL_WIN'.Replace('/','\\'), 'OrdinalIgnoreCase') } |
   ForEach-Object { \$_.ProcessId }" 2>/dev/null | tr -d '\r' | tr '\n' ' ')"
RUNNING="$(printf '%s' "$RUNNING" | sed 's/^ *//;s/ *$//')"
if [ -n "$RUNNING" ]; then
  # 勝手に落とすと作業中のものが失われるので、こちらからは止めない。
  if [ "$DRY_RUN" -eq 1 ]; then
    say "        動いている（PID: $RUNNING）。このまま試すと、ここで中止になる"
  else
    die "導入先の xyzzy が動いている（PID: $RUNNING）。閉じてからやり直すこと。"
  fi
else
  say "        動いていない"
fi

# --- 4. 退避する ----------------------------------------------------------
say "[4/7] 退避する"
if [ -e "$BACKUP_DIR" ]; then
  if [ "$REPLACE_BACKUP" -eq 1 ]; then
    act "退避先を捨てる: $BACKUP_DIR"
    [ "$DRY_RUN" -eq 1 ] || rm -rf "$BACKUP_DIR"
  elif [ "$DRY_RUN" -eq 1 ]; then
    say "        退避先が既にある: $BACKUP_DIR"
    say "        このまま試すと、ここで中止になる（--replace-backup で捨てられる）"
  else
    die "退避先が既にある: $BACKUP_DIR
中身を確かめて消すか、--replace-backup を付けること。前回の退避を黙って捨てない。"
  fi
fi
act "$INSTALL_DIR -> $BACKUP_DIR"
if [ "$DRY_RUN" -eq 0 ]; then
  cp -r "$INSTALL_DIR" "$BACKUP_DIR"
  [ $? -eq 0 ] || die "退避に失敗した"
  # 退避が完全であることを確かめてから先へ進む。ここから先は壊しにいく。
  before="$(cd "$INSTALL_DIR" && find . -type f | sort | wc -l)"
  after="$(cd "$BACKUP_DIR" && find . -type f | sort | wc -l)"
  [ "$before" = "$after" ] || die "退避が不完全（元 $before 個、退避先 $after 個）"
  BACKUP_DONE=1
  say "        $after 個を退避した"
fi

# --- 5. 被せる ------------------------------------------------------------
say "[5/7] 新しいものを被せる"
for exe in xyzzy.exe xyzzycli.exe xyzzyenv.exe; do
  act "$exe"
  [ "$DRY_RUN" -eq 1 ] || cp "$SRC/$exe" "$INSTALL_DIR/$exe" || die "$exe を置けない"
done
for dir in lisp etc; do
  act "$dir/ （上書きのみ。導入先の追加分は残す）"
  [ "$DRY_RUN" -eq 1 ] || cp -r "$SRC/$dir/." "$INSTALL_DIR/$dir/" || die "$dir を置けない"
done

# --- 6. 古い派生物を退ける ------------------------------------------------
say "[6/7] 古い派生物を退ける"
if [ -f "$INSTALL_DIR/xyzzy.wxp" ]; then
  act "ダンプを消す: xyzzy.wxp"
  [ "$DRY_RUN" -eq 1 ] || rm -f "$INSTALL_DIR/xyzzy.wxp"
else
  say "        ダンプは無い"
fi

if [ -d "$INSTALL_DIR/site-lisp" ]; then
  lc_list="$(cd "$INSTALL_DIR/site-lisp" && find . -name '*.lc' | sort)"
  if [ -n "$lc_list" ]; then
    n="$(printf '%s\n' "$lc_list" | wc -l)"
    act "site-lisp の .lc を $n 個 $LC_HOLD_DIR へ退ける"
    orphan=""
    while IFS= read -r f; do
      [ -n "$f" ] || continue
      [ -f "$INSTALL_DIR/site-lisp/${f%.lc}.l" ] || orphan="$orphan${f#./}"$'\n'
      if [ "$DRY_RUN" -eq 0 ]; then
        mkdir -p "$LC_HOLD_DIR/$(dirname "$f")"
        mv "$INSTALL_DIR/site-lisp/$f" "$LC_HOLD_DIR/$f"
      fi
    done <<< "$lc_list"
    if [ -n "$orphan" ]; then
      say ""
      say "        対の .l が無い .lc があった。作り直せないので配布元から取り直すこと:"
      printf '%s' "$orphan" | sed 's/^/          /'
    fi
  else
    say "        site-lisp に .lc は無い"
  fi
else
  say "        site-lisp が無い"
fi

# --- 7. 記録する ----------------------------------------------------------
say "[7/7] 適用した版を記録する"
act "$STAMP_NAME"
if [ "$DRY_RUN" -eq 0 ]; then
  {
    printf 'version=%s\n' "$NEW_VERSION"
    printf 'sha256=%s\n' "$NEW_SHA"
    printf 'commit=%s\n' "$NEW_COMMIT"
    printf 'applied=%s\n' "$(date '+%Y-%m-%d %H:%M:%S')"
  } > "$STAMP"
fi

say ""
if [ "$DRY_RUN" -eq 1 ]; then
  say "試行なので何も変えていない。"
  exit 0
fi

say "$NEW_VERSION にした。"
say ""
say "  * 次の起動で etc/DOC とダンプが作り直される（少し待たされ、窓が前に出る）"
say "  * site-lisp は .l から読まれる。.lc を作り直すと次から速くなる"
say "  * 退避は $BACKUP_DIR に残してある。落ち着いてから消すこと"
