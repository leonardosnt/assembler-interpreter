@echo off
if not exist "precompiled-headers/pch.h.gch" (
  echo Compiling headers
  gcc -o precompiled-headers/pch.h.gch src/pch.h
)
echo Compiling...
gcc %* -Wall -Wfatal-errors -I precompiled-headers -o interp.exe src/main.c