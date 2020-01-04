@echo off
if not exist "precompiled-headers/pch.h.gch" (
  echo Compiling headers
  gcc -o precompiled-headers/pch.h.gch src/pch.h
)

echo Compiling tests...
rm run_tests.exe
gcc -Wall -Wfatal-errors -I precompiled-headers -o run_tests.exe src/tests.c