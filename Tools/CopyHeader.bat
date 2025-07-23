SET "PROJECT_DIR=%~1"
SET "OUTPUT_DIR=%~2"

echo [INFO] 복사 시작: %PROJECT_DIR%\*.h → %OUTPUT_DIR%\

xcopy "%PROJECT_DIR%\*.h" "%OUTPUT_DIR%\" /Y /I

exit /b