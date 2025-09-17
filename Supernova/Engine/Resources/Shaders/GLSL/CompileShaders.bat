@echo off
setlocal enabledelayedexpansion

:: Check VULKAN_SDK
if "%VULKAN_SDK%"=="" (
    echo VULKAN_SDK environment variable is not set.
    echo Please install the Vulkan SDK and set the environment variable.
    pause
    exit /b
)

:: Set path to glslangValidator
set GLSLANG_VALIDATOR="%VULKAN_SDK%\Bin\glslangValidator.exe"

:: Check if glslangValidator exists
if not exist %GLSLANG_VALIDATOR% (
    echo glslangValidator.exe not found at %GLSLANG_VALIDATOR%
    pause
    exit /b
)

:: Recursively find all .vert files
for /R %%f in (*.vert) do (
    set "VERT_PATH=%%f"
    set "BASENAME=%%~nf"
    set "DIR=%%~dpf"

    set "FRAG_PATH=!DIR!!BASENAME!.frag"

    if exist "!FRAG_PATH!" (
        echo Compiling shaders in !DIR!!BASENAME!...

        :: Compile vertex shader
        %GLSLANG_VALIDATOR% -V "!VERT_PATH!" -o "!DIR!!BASENAME!_vert.spv"

        :: Compile fragment shader
        %GLSLANG_VALIDATOR% -V "!FRAG_PATH!" -o "!DIR!!BASENAME!_frag.spv"
    )
)

echo Finished compiling shaders.
pause
