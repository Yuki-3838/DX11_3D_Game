@echo off
rem ============================================================
rem  配布フォルダ(dist)を作る。
rem  Visual Studioが入っていないPCでも動かせる構成にする。
rem
rem  Release構成を使う理由:
rem    Debug構成はデバッグ版CRT(ucrtbased.dll / MSVCP140D.dll)を必要とし、
rem    それらはVisual Studioが入っているPCにしか存在しない(再頒布も不可)。
rem    Release構成なら再頒布可能CRT(VC++ 再頒布可能パッケージ)だけで動く。
rem ============================================================
setlocal

set ROOT=%~dp0
set DIST=%ROOT%dist

where msbuild >nul 2>nul
if errorlevel 1 (
    echo [error] msbuild が見つかりません。
    echo         「Developer Command Prompt for VS 2022」から実行してください。
    exit /b 1
)

echo [1/3] Release^|x64 をビルドします...
msbuild "%ROOT%DX11_3D_Game.sln" /t:DX11_3D_Game /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if errorlevel 1 (
    echo [error] ビルドに失敗しました。
    exit /b 1
)

echo [2/3] dist フォルダを作り直します...
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"

echo [3/3] 実行に必要なファイルをコピーします...
copy /y "%ROOT%x64\Release\DX11_3D_Game.exe" "%DIST%\" >nul
rem Release構成がリンクしているAssimp。実行ファイルと同じ場所に必要。
copy /y "%ROOT%assimp-vc142-mt.dll" "%DIST%\" >nul
rem シェーダーは実行時にコンパイルするのでソースごと必要。
xcopy /e /i /q /y "%ROOT%shader" "%DIST%\shader" >nul
rem モデル・モーション・テクスチャ・音は実行時に作業フォルダから読む。
xcopy /e /i /q /y "%ROOT%assets" "%DIST%\assets" >nul

echo.
echo 完了: %DIST%
echo.
echo 配布先のPCで必要なもの:
echo   - Windows 10/11 (64bit)
echo   - Microsoft Visual C++ 再頒布可能パッケージ (x64)
echo     https://aka.ms/vs/17/release/vc_redist.x64.exe
echo   - DirectX 11 が動くGPU
echo.
echo dist フォルダごと渡し、中の DX11_3D_Game.exe を実行してもらってください。
echo (exeだけを別の場所へ移すと assets / shader を見失って起動しません)
endlocal
