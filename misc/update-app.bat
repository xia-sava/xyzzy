@echo off
rem "配布されている最新版を取ってきて、導入済みの xyzzy を置き換える。"
rem ""
rem "  update-app.bat [選択肢] <導入先>"
rem ""
rem "    /dry-run    何をするかだけ表示し、一切変更しない"
rem "    /force      同じ版でも適用する"
rem "    /no-backup  退避を常に取らない。導入先の中だけで完結する"
rem "    /backup     退避を常に取る（既定は形式が変わる回だけ）"
rem "    /from-app   xyzzy から呼ばれた。終了を待ってから始め、終わったら開き直す"
rem ""
rem "Windows 同梱の道具だけで動く。curl・certutil・tar・robocopy・powershell。"
rem ""
rem "置き換えるのは実行ファイルと lisp・etc だけで、利用者のものには触らない。"
rem "docs と reference も触らない。"
rem ""
rem "古い .lc が site-lisp に残っていると、バイトコードの形式が変わった版では"
rem "起動が完了しない。.lc が無ければ .l が読まれるので、退けるだけで動く。"
rem "同じ理由でダンプも消す。exe を差し替えただけでは古いダンプが有効と判定される。"
rem ""
rem "このファイルは CP932 で保存する。cmd は .bat を CP932 として読むため、"
rem "UTF-8 で書くと日本語を出す行が壊れる。"

setlocal enabledelayedexpansion

set "DRY_RUN="
set "FORCE="
set "FROM_APP="
set "INSTALL="
set "BACKUP_DONE="
set "NO_BACKUP="
set "FORCE_BACKUP="
set "BASE_URL=%XYZZY_UPDATE_BASE_URL%"
if not defined BASE_URL set "BASE_URL=https://github.com/xia-sava/xyzzy/releases/download/latest"

set "SYS=%SystemRoot%\System32"
set "WAIT_LIMIT=60"

:parse
if "%~1"=="" goto parsed
rem "git bash から叩くと / 始まりの引数がパスに変換されるので、-- 形式も受ける。"
if /i "%~1"=="/dry-run" (set "DRY_RUN=1" & shift & goto parse)
if /i "%~1"=="--dry-run" (set "DRY_RUN=1" & shift & goto parse)
if /i "%~1"=="/force" (set "FORCE=1" & shift & goto parse)
if /i "%~1"=="--force" (set "FORCE=1" & shift & goto parse)
if /i "%~1"=="/no-backup" (set "NO_BACKUP=1" & shift & goto parse)
if /i "%~1"=="--no-backup" (set "NO_BACKUP=1" & shift & goto parse)
if /i "%~1"=="/backup" (set "FORCE_BACKUP=1" & shift & goto parse)
if /i "%~1"=="--backup" (set "FORCE_BACKUP=1" & shift & goto parse)
if /i "%~1"=="/from-app" (set "FROM_APP=1" & shift & goto parse)
if /i "%~1"=="--from-app" (set "FROM_APP=1" & shift & goto parse)
if /i "%~1"=="/?" goto usage
if /i "%~1"=="/help" goto usage
if /i "%~1"=="--help" goto usage
if defined INSTALL (call :die "導入先を 2 つ受け取った" & exit /b 1)
set "INSTALL=%~1"
shift
goto parse

:parsed
if not defined INSTALL set "INSTALL=%XYZZY_INSTALL_DIR%"
if not defined INSTALL goto usage

if not exist "%INSTALL%\xyzzy.exe" (
  call :die "xyzzy.exe が無いので導入先とは思えない: %INSTALL%"
  exit /b 1
)
for %%d in ("%INSTALL%") do set "INSTALL=%%~fd"

rem "導入先に置かれた自分自身も入れ替えるので、そこから起動されたときは一時領域へ"
rem "写して走り直す。cmd も走らせながら読むため、実行中のファイルを書き換えると壊れる。"
if /i "%~dp0"=="%INSTALL%\" if not defined XYZZY_UPDATE_RELOCATED (
  set "XYZZY_UPDATE_RELOCATED=1"
  copy /y "%~f0" "%TEMP%\xyzzy-update-run.bat" >nul || (call :die "自分自身を写せない" & exit /b 1)
  call "%TEMP%\xyzzy-update-run.bat" %*
  exit /b !errorlevel!
)

set "BACKUP=%INSTALL%.bak"
set "HOLD=%INSTALL%.lc-hold"
set "STAMP=%INSTALL%\.xyzzy-update"

set "WORK=%TEMP%\xyzzy-update-%RANDOM%%RANDOM%"
mkdir "%WORK%" 2>nul || (call :die "作業場所を作れない" & exit /b 1)

echo 導入先        %INSTALL%
echo 配布物の置き場 %BASE_URL%
echo.

rem "--- 1. 配布中の版を調べる ---------------------------------------------"
echo [1/7] 配布中の版を調べる
"%SYS%\curl.exe" -fsSL "%BASE_URL%/manifest.json" -o "%WORK%\manifest.json"
if errorlevel 1 (call :fail "manifest.json を取得できない" & exit /b 1)

call :json "%WORK%\manifest.json" version NEW_VERSION
call :json "%WORK%\manifest.json" asset   NEW_ASSET
call :json "%WORK%\manifest.json" sha256  NEW_SHA
call :json "%WORK%\manifest.json" commit  NEW_COMMIT
call :json "%WORK%\manifest.json" bytecode NEW_BYTECODE
if not defined NEW_VERSION (call :fail "manifest.json を読めない" & exit /b 1)
if not defined NEW_ASSET   (call :fail "manifest.json を読めない" & exit /b 1)
if not defined NEW_SHA     (call :fail "manifest.json を読めない" & exit /b 1)

set "OLD_VERSION="
set "OLD_SHA="
set "OLD_BYTECODE="
if exist "%STAMP%" (
  for /f "usebackq tokens=1,* delims==" %%a in ("%STAMP%") do (
    if /i "%%a"=="version" set "OLD_VERSION=%%b"
    if /i "%%a"=="sha256" set "OLD_SHA=%%b"
    if /i "%%a"=="bytecode" set "OLD_BYTECODE=%%b"
  )
)
if defined OLD_VERSION (
  echo         導入済み  !OLD_VERSION!
) else (
  echo         導入済み  不明（このスクリプトで更新した記録が無い）
)
echo         配布中    %NEW_VERSION%

rem "バイトコードの形式が変わっていなければ、古い .lc はそのまま使える。形式は特殊"
rem "形式の名前で表され、改名されると古い .lc が読めなくなる。名前が同じなら触らない。"
rem "どちらかが分からないときは、安全側に倒して退ける。"
set "KEEP_LC="
if defined OLD_BYTECODE if defined NEW_BYTECODE if /i "!OLD_BYTECODE!"=="!NEW_BYTECODE!" set "KEEP_LC=1"

if defined OLD_SHA if /i "!OLD_SHA!"=="%NEW_SHA%" if not defined FORCE (
  echo.
  echo 配布中のものが既に入っている。/force で強制できる。
  call :cleanup
  exit /b 0
)

rem "--- 2. 落として照合する -----------------------------------------------"
echo [2/7] 配布物を落として照合する
"%SYS%\curl.exe" -fsSL "%BASE_URL%/%NEW_ASSET%" -o "%WORK%\%NEW_ASSET%"
if errorlevel 1 (call :fail "%NEW_ASSET% を取得できない" & exit /b 1)

rem "for /f の中では実行ファイルを引用符で囲めない。囲むと構文エラーになる。"
set "GOT_SHA="
for /f "usebackq skip=1 delims=" %%h in (`%SYS%\certutil.exe -hashfile "%WORK%\%NEW_ASSET%" SHA256`) do (
  if not defined GOT_SHA set "GOT_SHA=%%h"
)
set "GOT_SHA=%GOT_SHA: =%"
if /i not "%GOT_SHA%"=="%NEW_SHA%" (
  call :fail "照合に失敗した（sha256 が合わない）"
  exit /b 1
)
echo         sha256 一致

mkdir "%WORK%\x" 2>nul
pushd "%WORK%\x"
"%SYS%\tar.exe" -xf "%WORK%\%NEW_ASSET%"
set "RC=!errorlevel!"
popd
if not "!RC!"=="0" (call :fail "配布物を展開できない" & exit /b 1)

set "SRC=%WORK%\x\xyzzy"
for %%f in (xyzzy.exe xyzzycli.exe xyzzyenv.exe) do (
  if not exist "%SRC%\%%f" (call :fail "配布物に %%f が無い" & exit /b 1)
)
if not exist "%SRC%\lisp" (call :fail "配布物に lisp が無い" & exit /b 1)
if not exist "%SRC%\etc" (call :fail "配布物に etc が無い" & exit /b 1)
echo         展開した

rem "--- 3. 動いていないことを確かめる -------------------------------------"
echo [3/7] 動いていないことを確かめる
call :running COUNT
rem "数えられなかったときに「動いていない」と黙って通さない。"
if "!COUNT!"=="x" (call :fail "動作中かどうかを確かめられなかった。何も変更していない。" & exit /b 1)
if not "!COUNT!"=="0" if defined FROM_APP if not defined DRY_RUN (
  echo         終了を待っている
  set /a WAITED=0
  :waitloop
  "%SYS%\ping.exe" -n 2 127.0.0.1 >nul
  set /a WAITED+=1
  call :running COUNT
  if not "!COUNT!"=="0" if !WAITED! lss %WAIT_LIMIT% goto waitloop
  if not "!COUNT!"=="0" (
    call :fail "%WAIT_LIMIT% 秒待っても終了しなかった。何も変更していない。"
    exit /b 1
  )
  echo         !WAITED! 秒で終了した
)
call :running COUNT
if not "!COUNT!"=="0" (
  if defined DRY_RUN (
    echo         動いている。このまま試すと、ここで中止になる
  ) else (
    call :fail "導入先の xyzzy が動いている。閉じてからやり直すこと。"
    exit /b 1
  )
) else (
  echo         動いていない
)

rem "--- 4. 退避する -------------------------------------------------------"
echo [4/7] 退避する
rem "退避を取るのはバイトコードの形式が変わる回だけにする。その回は site-lisp の .lc"
rem "を退けて作り直すので、起動できなくなる余地がある。形式が変わらない回は実行ファイル"
rem "と lisp・etc を置き換えるだけで、戻したければ版ごとのリリースから取り直せる。"
rem ""
rem "残すのは直前の姿ひとつ。/no-backup で常に取らない、/backup で常に取る。"
if defined NO_BACKUP (
  echo         退避を取らない（/no-backup）
  goto overlay
)
if defined KEEP_LC if not defined FORCE_BACKUP (
  echo         退避を取らない（バイトコードの形式が変わらないため）
  goto overlay
)
if exist "%BACKUP%" (
  call :act "前の退避を捨てる: %BACKUP%"
  if not defined DRY_RUN rmdir /s /q "%BACKUP%"
)
call :act "%INSTALL% から %BACKUP% へ"
if not defined DRY_RUN (
  robocopy "%INSTALL%" "%BACKUP%" /E /NJH /NJS /NP /NFL /NDL >nul
  if !errorlevel! geq 8 (call :fail "退避に失敗した" & exit /b 1)
  rem "退避が完全であることを確かめてから先へ進む。ここから先は壊しにいく。"
  call :count "%INSTALL%" BEFORE
  call :count "%BACKUP%" AFTER
  if not "!BEFORE!"=="!AFTER!" (
    call :fail "退避が不完全（元 !BEFORE! 個、退避先 !AFTER! 個）"
    exit /b 1
  )
  set "BACKUP_DONE=1"
  echo         !AFTER! 個を退避した
)

:overlay
rem "--- 5. 被せる ---------------------------------------------------------"
echo [5/7] 新しいものを被せる
for %%f in (xyzzy.exe xyzzycli.exe xyzzyenv.exe) do (
  call :act "%%f"
  if not defined DRY_RUN copy /y "%SRC%\%%f" "%INSTALL%\%%f" >nul || (call :fail "%%f を置けない" & exit /b 1)
)
for %%d in (lisp etc) do (
  call :act "%%d\ （上書きのみ。導入先の追加分は残す）"
  if not defined DRY_RUN (
    robocopy "%SRC%\%%d" "%INSTALL%\%%d" /E /NJH /NJS /NP /NFL /NDL >nul
    if !errorlevel! geq 8 (call :fail "%%d を置けない" & exit /b 1)
  )
)
rem "自分自身も配布物のものへ入れ替える。手順に不備があっても次の更新で直せる。"
rem "いま走っているのは一時領域へ写した方なので、実体を書き換えても差し支えない。"
if exist "%SRC%\update-app.bat" (
  call :act "update-app.bat （更新の手順そのもの）"
  if not defined DRY_RUN copy /y "%SRC%\update-app.bat" "%INSTALL%\update-app.bat" >nul
)

rem "--- 6. 古い派生物を退ける ---------------------------------------------"
echo [6/7] 古い派生物を退ける
if exist "%INSTALL%\xyzzy.wxp" (
  call :act "ダンプを消す: xyzzy.wxp"
  if not defined DRY_RUN del /q "%INSTALL%\xyzzy.wxp"
) else (
  echo         ダンプは無い
)

rem "配布物に .lc は入っていないので、導入先の lisp に残っている .lc は前の版のもの。"
rem "xyzzy は .lc を優先して読むため、残したままだと新しい .l ではなく古い .lc が"
rem "読まれ、入れ替えたはずの定義が効かない。消す先は lisp の下の .lc だけ。"
call :count_lc "%INSTALL%\lisp" LISPLC
if not "!LISPLC!"=="0" (
  call :act "lisp の古い .lc を !LISPLC! 個消す"
  if not defined DRY_RUN del /s /q "%INSTALL%\lisp\*.lc" >nul 2>&1
) else (
  echo         lisp に古い .lc は無い
)
set "MOVED_LC="
if defined KEEP_LC (
  echo         site-lisp の .lc はそのまま使える（バイトコードの形式は !NEW_BYTECODE! のまま）
) else if exist "%INSTALL%\site-lisp" (
  call :count_lc "%INSTALL%\site-lisp" LCCOUNT
  if not "!LCCOUNT!"=="0" (
    call :act "site-lisp の .lc を !LCCOUNT! 個 %HOLD% へ退ける"
    if not defined DRY_RUN (
      robocopy "%INSTALL%\site-lisp" "%HOLD%" *.lc /S /MOVE /NJH /NJS /NP /NFL /NDL >nul
      if !errorlevel! geq 8 (call :fail "site-lisp の .lc を退けられない" & exit /b 1)
      set "MOVED_LC=1"
    )
  ) else (
    echo         site-lisp に .lc は無い
  )
) else (
  echo         site-lisp が無い
)

rem "--- 7. 記録する -------------------------------------------------------"
echo [7/7] 適用した版を記録する
call :act ".xyzzy-update"
if not defined DRY_RUN (
  > "%STAMP%" echo version=%NEW_VERSION%
  >>"%STAMP%" echo sha256=%NEW_SHA%
  >>"%STAMP%" echo commit=%NEW_COMMIT%
  >>"%STAMP%" echo bytecode=%NEW_BYTECODE%
  >>"%STAMP%" echo applied=%DATE% %TIME%
)

call :cleanup
echo.
if defined DRY_RUN (
  echo 試行なので何も変えていない。
  exit /b 0
)

echo %NEW_VERSION% にした。
echo.
echo   * 次の起動で etc\DOC とダンプが作り直される（少し待たされ、窓が前に出る）
if defined BACKUP_DONE (
  echo   * 直前の姿は %BACKUP% に残っている。それより古い版はリリースから取り直せる
) else (
  echo   * 退避は取っていない。戻すなら版ごとのリリースから取り直す
)
rem "xyzzy から呼ばれた回は開き直しのついでに作り直すので、ここでは言わない。"
if defined MOVED_LC if not defined FROM_APP (
  echo   * site-lisp の .lc を退けた。M-x update-rebuild-site-lisp で作り直せる
  echo     ^(作り直さなくても .l が読まれる。起動が遅くなるだけ^)
)

rem "xyzzy から呼ばれたのなら、終わらせた当人を開き直す。退けた .lc があれば、"
rem "開き直したついでに作り直させる。"
if defined FROM_APP (
  echo.
  echo xyzzy を開き直す。
  if defined MOVED_LC (
    start "" "%INSTALL%\xyzzy.exe" -e "(ed::update-rebuild-site-lisp)"
  ) else (
    start "" "%INSTALL%\xyzzy.exe"
  )
)
exit /b 0

rem "--- 小道具 -------------------------------------------------------------"

:usage
echo 配布されている最新版を取ってきて、導入済みの xyzzy を置き換える。
echo.
echo   update-app.bat [選択肢] ^<導入先^>
echo.
echo     /dry-run    何をするかだけ表示し、一切変更しない
echo     /force      同じ版でも適用する
echo     /no-backup  退避を常に取らない。導入先の中だけで完結する
echo     /backup     退避を常に取る（既定は形式が変わる回だけ）
echo     /from-app   xyzzy から呼ばれた。終了を待ってから始め、終わったら開き直す
echo.
echo   導入先は環境変数 XYZZY_INSTALL_DIR でも渡せる。
exit /b 1

:die
echo update-app: %~1 1>&2
if defined BACKUP_DONE (
  echo. 1>&2
  echo 導入先が中途半端になっている。退避から戻すこと: 1>&2
  echo   rmdir /s /q "%INSTALL%" ^&^& move "%BACKUP%" "%INSTALL%" 1>&2
)
exit /b 1

:fail
call :cleanup
call :die "%~1"
exit /b 1

:act
if defined DRY_RUN (echo         ^(試行^) %~1) else (echo         %~1)
exit /b 0

:cleanup
if defined WORK if exist "%WORK%" rmdir /s /q "%WORK%" 2>nul
exit /b 0

rem "平らな JSON から文字列の値を取り出す。読む相手は自分たちが書いた"
rem "manifest.json だけなので、形が決まっていることに頼ってよい。"
:json
setlocal enabledelayedexpansion
set "_v="
for /f "usebackq tokens=2 delims=:" %%a in (`%SYS%\findstr.exe /c:"\"%~2\"" "%~1"`) do set "_v=%%a"
if defined _v (
  set "_v=!_v:"=!"
  set "_v=!_v:,=!"
  for /f "tokens=* delims= " %%b in ("!_v!") do set "_v=%%b"
)
endlocal & set "%~3=%_v%"
exit /b 0

rem "導入先の中から起動されている xyzzy の数を数える。"
rem ""
rem "for /f の丸括弧の中では使えない。式に含まれる丸括弧を cmd が数えてしまい、"
rem "構文が壊れる。答えは一旦ファイルへ落として読む。式にはパイプも二重引用符も"
rem "使わない。どちらも逃がし方を誤ると黙って壊れる。"
:running
setlocal
set "_ps=$n=0; foreach ($x in (Get-CimInstance Win32_Process)) { if ($x.Name -eq 'xyzzy.exe' -or $x.Name -eq 'xyzzycli.exe' -or $x.Name -eq 'xyzzyenv.exe') { if ($x.ExecutablePath -and $x.ExecutablePath.StartsWith('%INSTALL%','OrdinalIgnoreCase')) { $n++ } } }; $n"
set "_out=%WORK%\running.txt"
powershell.exe -NoProfile -NonInteractive -Command "%_ps%" > "%_out%" 2>nul
rem "set /p を if と同じ行に書くと、< が if 側に付いて構文が壊れる。"
set "_c="
for /f "usebackq delims=" %%n in ("%_out%") do if not defined _c set "_c=%%n"
if not defined _c set "_c=x"
endlocal & set "%~1=%_c%"
exit /b 0

:count
setlocal
set "_n=0"
for /f %%c in ('dir /s /b /a-d "%~1" 2^>nul ^| %SYS%\find.exe /c /v ""') do set "_n=%%c"
endlocal & set "%~2=%_n%"
exit /b 0

:count_lc
setlocal
set "_n=0"
for /f %%c in ('dir /s /b "%~1\*.lc" 2^>nul ^| %SYS%\find.exe /c /v ""') do set "_n=%%c"
endlocal & set "%~2=%_n%"
exit /b 0
