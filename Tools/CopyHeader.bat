SET "PROJECT_DIR=%~1"
SET "OUTPUT_DIR=%~2"

xcopy "%PROJECT_DIR%\*.h" "%OUTPUT_DIR%\" /Y /I >nul