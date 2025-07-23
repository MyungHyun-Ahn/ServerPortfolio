setlocal enabledelayedexpansion

rem ���� ����
set "TARGET_LIB=%~1"
set "SOLUTION_DIR=%~2"
set "PROJECT_LIST=%~3"

rem ��� ���� \ ������ ���̱�
if not "%SOLUTION_DIR:~-1%"=="\" set "SOLUTION_DIR=%SOLUTION_DIR%\"

echo [INFO] Ÿ�� Lib: %TARGET_LIB%
echo [INFO] �ַ�� ���͸�: %SOLUTION_DIR%
echo [INFO] ������Ʈ ����Ʈ ����: %PROJECT_LIST%

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
echo [INFO] ��� ���� �۾� �Ϸ�.
endlocal
exit /b 0