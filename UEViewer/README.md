Setup of UEViewer
- git clone git@github.com:gildor2/UEViewer.git
- `t.bat`
- Download https://github.com/gildor2/BuildTools
- `perl.exe ..\UEViewer\Unreal\Shaders\make.pl`

Patch of UEViewer
- UmodelTool/Build.h,
    - `#define VSTUDIO_INTEGRATION		0`
- Core/Core.h
    - `#if !WIN32_USE_SEH` -> `#if 1 || !WIN32_USE_SEH`

