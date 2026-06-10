@echo off
C:\msys64\mingw64\bin\gcc.exe %1 -o %~n1 ^
-IC:/msys64/mingw64/include ^
-LC:/msys64/mingw64/lib ^
-lmingw32 -lSDL2main -lSDL2 -lSDL2_image ^
-lavformat -lavcodec -lavutil -lswscale -lswresample