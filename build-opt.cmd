@echo off
if not exist "precompiled-headers/pch.h.gch" (
  echo Compiling headers
  gcc -o precompiled-headers/pch.h.gch src/pch.h
)
echo Compiling...
gcc -O3 -Wall -Wfatal-errors -I precompiled-headers -o interp-opt.exe src/main.c