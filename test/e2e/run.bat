@echo off
rem End-to-end suite for the Windows targets: every case is compiled with the
rem compiler that was built from this tree, then run, and its output and exit
rem code are compared with the expected ones. Nothing is left behind but the
rem temporary directory, which this script removes at the end.
rem
rem The compiler comes from SHAOLANG, and the Visual Studio tools have to be
rem on the PATH, the same way the compiler needs them, so run this from a
rem developer command prompt.
rem
rem Usage: run.bat

setlocal enabledelayedexpansion

set "CASES=%~dp0"

if "%SHAOLANG%"=="" (
    echo FAIL SHAOLANG is not set: point it at the compiler to test
    exit /b 1
)

rem The compiler shells out to these, so a missing tool fails every case the
rem same way. The assembler depends on the compiler's own word size, so
rem either one is enough here.
set "HAS_ASSEMBLER=0"
where ml64.exe >nul 2>&1
if not errorlevel 1 set "HAS_ASSEMBLER=1"
where ml.exe >nul 2>&1
if not errorlevel 1 set "HAS_ASSEMBLER=1"
if "%HAS_ASSEMBLER%"=="0" (
    echo FAIL neither ml64.exe nor ml.exe is on the PATH
    exit /b 1
)
where link.exe >nul 2>&1
if errorlevel 1 (
    echo FAIL link.exe is not on the PATH
    exit /b 1
)

set "WORK=%TEMP%\shaolang-e2e-%RANDOM%%RANDOM%"
if not exist "%WORK%" mkdir "%WORK%"
if not exist "%WORK%" (
    echo FAIL cannot create the working directory %WORK%
    exit /b 1
)

set "PASSED=0"
set "FAILED=0"

for %%f in ("%CASES%*.sl") do call :run_case "%%~nF" "%CASES%%%~nF.sl" "%CASES%%%~nF.expected"

rem The deep-frame cases are not committed: the number of locals decides the
rem frame size, which is what exercises the addressing beyond the small
rem immediate range. The expected sum is computed here, not taken from the
rem compiler.
call :gen_deep 300
call :run_case deepFrame300 "%WORK%\deepFrame300.sl" "%WORK%\deepFrame300.expected"
call :gen_deep 600
call :run_case deepFrame600 "%WORK%\deepFrame600.sl" "%WORK%\deepFrame600.expected"

echo.
echo passed %PASSED%, failed %FAILED%
rmdir /s /q "%WORK%" >nul 2>&1
if "%FAILED%"=="0" (
    exit /b 0
)
exit /b 1

rem Compile one case, run it, and compare its output and exit code.
:run_case
set "NAME=%~1"
set "SRC=%~2"
set "EXPECTED=%~3"
set "EXE=%WORK%\%NAME%.exe"
set "OUT=%WORK%\%NAME%.out"
set "LOG=%WORK%\%NAME%.log"

"%SHAOLANG%" "%SRC%" -o "%EXE%" > "%LOG%" 2>&1
if errorlevel 1 (
    echo FAIL %NAME% : the compiler failed
    type "%LOG%"
    set /a FAILED+=1
    exit /b 0
)
if not exist "%EXE%" (
    echo FAIL %NAME% : no executable was produced
    type "%LOG%"
    set /a FAILED+=1
    exit /b 0
)

set "INPUT=%CASES%%NAME%.in"
if not exist "%INPUT%" set "INPUT=NUL"
"%EXE%" < "%INPUT%" > "%OUT%" 2>&1
set "CODE=!ERRORLEVEL!"

rem The expected files are checked out with CRLF, which is what the program
rem writes on Windows, so the two are compared as they are.
fc /b "%EXPECTED%" "%OUT%" > "%WORK%\%NAME%.diff" 2>&1
if errorlevel 1 (
    echo FAIL %NAME% : the output differs
    type "%WORK%\%NAME%.diff"
    set /a FAILED+=1
    exit /b 0
)

if exist "%CASES%%NAME%.exit" (
    set "WANT="
    for /f "usebackq tokens=* delims=" %%c in ("%CASES%%NAME%.exit") do set "WANT=%%c"
    if not "!CODE!"=="!WANT!" (
        echo FAIL %NAME% : exit code !CODE!, expected !WANT!
        set /a FAILED+=1
        exit /b 0
    )
)

echo ok   %NAME%
set /a PASSED+=1
exit /b 0

rem Write a case with the given number of locals, and the sum they add up to.
:gen_deep
set "N=%~1"
set "NAME=deepFrame%N%"
set "EXPR=  output 0"
> "%WORK%\%NAME%.sl" echo -[int] main(){
for /l %%i in (1,1,%N%) do (
    >> "%WORK%\%NAME%.sl" echo   int v%%i = %%i;
    set "EXPR=!EXPR! + v%%i"
)
>> "%WORK%\%NAME%.sl" echo !EXPR!;
>> "%WORK%\%NAME%.sl" echo   output "\n";
>> "%WORK%\%NAME%.sl" echo   return 0;
>> "%WORK%\%NAME%.sl" echo }
set /a SUM=0
for /l %%i in (1,1,%N%) do set /a SUM+=%%i
> "%WORK%\%NAME%.expected" echo !SUM!
exit /b 0
