@echo off 
SET includes=/Iext /Iext\freetype\include /Iext\glad\include 
SET sources=src\win32_codex.cpp ext\glad\src\glad.c src\xpath.cpp
SET warning_flags=/W4 /wd4100 /wd4101 /wd4189 /wd4996 /wd4530 /wd4201 /wd4505 /wd4098 /wd4700 /wd4127 /wd4310
SET compiler_flags=/nologo /FC /MDd /Zi %warning_flags% %includes% /Fe:Codex.exe /Fo:build\ /Fdbuild\
SET LINKER_FLAGS=/SUBSYSTEM:CONSOLE /OPT:REF /INCREMENTAL:NO /debug /IGNORE:4098 /IGNORE:4099 /LIBPATH:ext\freetype\ freetype.lib opengl32.lib user32.lib gdi32.lib winmm.lib shell32.lib winmm.lib shell32.lib shlwapi.lib

IF NOT EXIST build MKDIR build
CL %compiler_flags% %sources% /link %linker_flags%

rem del *.obj > nul
