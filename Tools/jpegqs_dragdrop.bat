@echo off
setlocal enabledelayedexpansion

:: === JPEGQS Drag-and-Drop Tool ===
:: Place this .bat file in the same folder as jpegqs.exe
:: Drag JPEG files onto this file to upscale and denoise them
:: Output will be saved in the same folder with "_upscale" suffix

:: Check if executable exists
if not exist "jpegqs.exe" (
    echo [ERROR] jpegqs.exe not found in this folder!
    pause
    exit /b
)

:: Loop through each file dropped
for %%F in (%*) do (
    set "INPUT=%%~fF"
    set "EXT=%%~xF"
    set "NAME=%%~nF"
    set "FOLDER=%%~dpF"
    
    set "OUTPUT=!FOLDER!!NAME!_upscale!EXT!"

    echo Processing: !INPUT!
    echo Output will be: !OUTPUT!

    :: Run jpegqs with quality=5, 3 iterations, optimized output
    jpegqs.exe --quality 5 --niter 3 --optimize "!INPUT!" "!OUTPUT!"

    if exist "!OUTPUT!" (
        echo [SUCCESS] Saved: !OUTPUT!
    ) else (
        echo [FAILED] Could not create output for: !INPUT!
    )
    echo -----------------------------
)

echo All done.
pause
