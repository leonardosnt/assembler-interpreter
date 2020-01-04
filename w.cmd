rm interp.exe

filewatch src/ -e "cls & build.cmd && interp.exe codes/test.asm & echo Program exited"