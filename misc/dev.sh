#!/usr/bin/env bash
# xyzzy のビルド・バイトコンパイル・テストを Bash から安定して実行する。
#
#   misc/dev.sh build [Release|Debug] [Build|Rebuild]
#   misc/dev.sh bytecompile
#   misc/dev.sh test [visible [all]]
#   misc/dev.sh reference
#   misc/dev.sh all
#
# この環境固有の事情を吸収するのがこのスクリプトの目的:
#
#   - bytecompile と test は xyzzy を起動する。既定では別デスクトップで走らせるので
#     利用者の画面には窓が出ない。対話デスクトップを要求する OLE の4件だけが落ちる。
#   - test visible はその4件だけを画面で走らせる（数十秒）。既定の実行と合わせて全件になる。
#     どちらも画面を占有するので断ってから。全件を画面で走らせるなら test visible all。
#   - NoDefaultCurrentDirectoryInExePath=1 が設定されており、実行ファイルも
#     バッチファイルもカレントディレクトリからは見つからない。全てフルパスで起動する。
#   - ドライブレターは大文字にする。si:system-root は GetModuleFileName 由来で起動時の
#     綴りを保つ一方、(cwd) は cmd 由来で大文字になるため、小文字で起動すると
#     process-command-line--s / --S が string= で外れて落ちる。
#   - カレントディレクトリをリポジトリ直下に固定する。上記テストは (cwd) と
#     si:system-root が一致することを前提にしている。

set -u

REPO_UNIX="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_UNIX" || exit 1

WORK="$REPO_UNIX/.claude/work.local.d"
mkdir -p "$WORK"

# /c/foo -> C:/foo （ドライブレターは大文字へ）
to_win () {
  local p="$1"
  if [[ "$p" =~ ^/([a-zA-Z])/(.*)$ ]]; then
    local d="${BASH_REMATCH[1]}"
    printf '%s:/%s' "${d^^}" "${BASH_REMATCH[2]}"
  else
    printf '%s' "$p"
  fi
}

# C:/foo -> C:\foo
to_bs () { printf '%s' "${1//\//\\}"; }

REPO_WIN="$(to_win "$REPO_UNIX")"
WORK_WIN="$(to_win "$WORK")"
REPO_BS="$(to_bs "$REPO_WIN")"
WORK_BS="$(to_bs "$WORK_WIN")"

# cl / msbuild / cmd の出力は CP932 かつ CRLF。UTF-8 の行が混ざることもあるので -c で捨てる。
decode () { iconv -f CP932 -t UTF-8 -c < "$1" | dos2unix; }

# xyzzy を起動する処理は、利用者の画面に窓が出ないよう別デスクトップで走らせる。
# 子プロセスは親のデスクトップを引き継ぐので、cmd をそこで起動すれば xyzzy も従う。
# デスクトップを作れなかった場合は、見える方へ落とさずに失敗する。
offscreen () {
  MSYS_NO_PATHCONV=1 python "$REPO_WIN/misc/verify/rundesk.py" \
    cmd.exe /c "$1"
}

do_build () {
  local config="${1:-Release}" target="${2:-Build}"
  local log="$WORK/build.log"
  cmd //c "$REPO_BS\\build.bat $config $target minimal" > "$log" 2>&1
  local rc=$?
  local warn err
  warn=$(grep -ci 'warning' "$log")
  err=$(grep -ci 'error' "$log")
  echo "build   : exit=$rc  warning=$warn  error=$err"
  if [ "$rc" -ne 0 ] || [ "$warn" -ne 0 ] || [ "$err" -ne 0 ]; then
    decode "$log" | grep -Ei 'warning|error' | head -20
    return 1
  fi
}

do_bytecompile () {
  local log="$WORK/bytecompile.log"
  offscreen "$REPO_BS\\bytecompile.bat" > "$log" 2>&1
  local rc=$?
  local n
  n=$(ls "$REPO_UNIX"/lisp/*.lc 2>/dev/null | wc -l)
  echo "bytecomp: exit=$rc  lc=$n"
  if [ "$rc" -ne 0 ]; then
    decode "$log" | tail -20
    return 1
  fi
}

# テストは cmd 経由で起動する。test-environ-1 / -2 が cmd.exe /c set の出力と si:environ を
# 突き合わせるため、親の環境に PROMPT が無いと2件だけ落ちる（cmd は起動時に自分の環境へ
# PROMPT を足すので、xyzzy 側にだけ存在しない変数ができてしまう）。
#
# XYZZYCONFIGPATH は毎回空にした wxp という名前のディレクトリを指す。使い回すと前回の
# 最小化状態が復元されて split-window 系が落ち、名前が wxp でないと
# user-config-path-and-dump-image-path-8-test が落ちる。
# 別デスクトップでは InternetExplorer.Application が起動できない（COM が対話デスクトップを
# 要求する）。この4件だけは画面を使う実行でしか通らない。
OLE_NEEDS_DESKTOP='test-ole-method*-args
test-ole-method*-named-args
fix-ole-event-sink-load-typelib
fix-ole-for-each'

do_test () {
  local where="${1:-offscreen}" scope="${2:-}"
  local cfg="$WORK/testcfg/wxp"
  local out="$WORK/test-results.txt"

  rm -rf "$WORK/testcfg"
  mkdir -p "$cfg"
  rm -f "$out"

  if [ "$where" = visible ] && [ "$scope" != all ]; then
    # 画面を使う実行は、画面を必要とする4件だけに絞る。残りは既定の実行が受け持つので、
    # 二つ合わせて全件を通したことになる。
    cat > "$WORK/run-tests.l" <<EOF
(let ((dir "$REPO_WIN/unittest/")
      (out "$WORK_WIN/test-results.txt"))
  (load-file (merge-pathnames "simple-test.l" dir))
  (load-test-file (merge-pathnames "ole-tests.l" dir))
  (run-tests
   (remove-if-not
    #'(lambda (x)
        (member (symbol-name (car x))
                '("test-ole-method*-args" "test-ole-method*-named-args"
                  "fix-ole-event-sink-load-typelib" "fix-ole-for-each")
                :test #'string-equal))
    (reverse *tests*)))
  (let ((buf (find-buffer "*Test Results*")))
    (when buf
      (save-excursion
        (set-buffer buf)
        (write-region (point-min) (point-max) out)))))
EOF
  else
    cat > "$WORK/run-tests.l" <<EOF
(let ((dir "$REPO_WIN/unittest/")
      (out "$WORK_WIN/test-results.txt"))
  (load-file (merge-pathnames "simple-test.l" dir))
  (let ((*error-on-overwriting-test* t))
    (dolist (test (sort (directory dir :absolute t :recursive t :wild "*-tests.l")
                        #'string-lessp))
      (load-test-file test)))
  (run-all-tests)
  (let ((buf (find-buffer "*Test Results*")))
    (when buf
      (save-excursion
        (set-buffer buf)
        (write-region (point-min) (point-max) out)))))
EOF
  fi

  cat > "$WORK/run-tests.bat" <<EOF
@echo off
cd /d "$REPO_BS"
set XYZZYCONFIGPATH=$WORK_BS\\testcfg\\wxp
"$REPO_BS\\xyzzy.exe" -q -trace -l "$WORK_BS\\run-tests.l" -kill
EOF
  unix2dos -q "$WORK/run-tests.bat"

  if [ "$where" = visible ]; then
    cmd //c "$WORK_BS\\run-tests.bat"
  else
    offscreen "$WORK_BS\\run-tests.bat"
  fi
  local rc=$?

  if [ ! -f "$out" ]; then
    echo "test    : exit=$rc  結果ファイルが作られなかった"
    return 1
  fi
  echo "test    : exit=$rc  $(decode "$out" | grep -E '^Total [0-9]+ tests' | head -1)"

  local failed unexpected
  failed=$(decode "$out" | sed -n 's/^\(.*\)\.\.\.Failed$/\1/p')
  if [ "$where" = visible ]; then
    unexpected="$failed"
  else
    unexpected=$(printf '%s\n' "$failed" | grep -Fxv -e "$OLE_NEEDS_DESKTOP" | grep -v '^$')
    if [ -n "$failed" ]; then
      echo "          画面を使わない実行では OLE が落ちる。全部通すには test visible"
    fi
  fi
  if [ -n "$unexpected" ]; then
    printf '%s\n' "$unexpected" | sed 's/^/          Failed: /'
    return 1
  fi
}

do_reference () {
  python "$REPO_WIN/misc/gen-reference-skill.py" --check
}

case "${1:-all}" in
  build)       shift; do_build "$@" ;;
  bytecompile) do_bytecompile ;;
  test)        shift; do_test "$@" ;;
  reference)   do_reference ;;
  all)         do_build && do_bytecompile && do_test && do_reference ;;
  *)           echo "usage: $0 {build|bytecompile|test|reference|all}" >&2; exit 2 ;;
esac
