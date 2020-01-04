cd src

gcc -fprofile-arcs -ftest-coverage -o interp_cov main.c
interp_cov.exe
gcov -b -c -n main.c

rm interp_cov.exe
rm -r *.gcov
rm -r *.gcda
rm -r *.gcno

cd..