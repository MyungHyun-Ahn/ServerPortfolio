SET "PROJECT_DIR=%~1"
SET "OUTPUT_DIR=%~2"

echo [INFO] ���� ����: %PROJECT_DIR%\*.h �� %OUTPUT_DIR%\

xcopy "%PROJECT_DIR%\*.h" "%OUTPUT_DIR%\" /Y /I

exit /b