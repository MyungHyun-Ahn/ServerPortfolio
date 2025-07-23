setlocal enabledelayedexpansion

rem 변수 설정
set "TARGET_LIB=%~1"
set "SOLUTION_DIR=%~2"
set "PROJECT_LIST=%~3"

rem 경로 끝에 \ 없으면 붙이기
if not "%SOLUTION_DIR:~-1%"=="\" set "SOLUTION_DIR=%SOLUTION_DIR%\"

echo [INFO] 타겟 Lib: %TARGET_LIB%
echo [INFO] 솔루션 디렉터리: %SOLUTION_DIR%
echo [INFO] 프로젝트 리스트 파일: %PROJECT_LIST%

for /f "usebackq delims=" %%P in ("%PROJECT_LIST%") do (
    set "PROJECT_PATH=%%P"
    if not "!PROJECT_PATH!"=="" (
        set "FULL_PATH=%SOLUTION_DIR%!PROJECT_PATH!"
        if not "!FULL_PATH:~-1!"=="\" set "FULL_PATH=!FULL_PATH!\"
        copy /Y "%TARGET_LIB%" "!FULL_PATH!"
        )
    )
)

echo.
echo [INFO] 모든 복사 작업 완료.
endlocal
exit /b 0