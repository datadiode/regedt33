@echo off

setlocal

set "PLATFORM=%~1"
set "CONFIGURATION=%~2"

call "GIT-VS-VERSION-GEN.bat" . "gen-versioninfo.h"

findstr /c:"ActiveCfg = %CONFIGURATION%|%PLATFORM%" /e regedt33_vc9.sln
if errorlevel 1 (
  call "%VS140COMNTOOLS%vsvars32.bat"
  msbuild /t:Rebuild regedt33.vcxproj /p:SolutionDir=%~dp0 /p:Platform="%PLATFORM%" /p:Configuration="%CONFIGURATION%"
) else (
  call "%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"
  vcbuild /rebuild regedt33_vc9.sln "%CONFIGURATION%|%PLATFORM%"
)

endlocal
