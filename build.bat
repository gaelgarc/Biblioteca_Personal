@echo off
if not exist datos mkdir datos
if exist output\main.exe del output\main.exe
gcc main.c -o output\main.exe -IC:\tools\raylib\include -LC:\tools\raylib\lib -lraylib -lopengl32 -lgdi32 -lwinmm
.\output\main.exe
pause