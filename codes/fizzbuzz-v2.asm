; kinda equivalent to this (in C):
; int f = 0;
; if (i % 3 == 0) f += 1;
; if (i % 5 == 0) f += 2;
;
; if (f == 0) {
;   printf("%d\n", i);
;   continue;
; }
; printf("fizz");
; if (f == 3) {
;   printf("buzz");
; }
; printf("\n");

; XXX TODO: this is not working correcly...

mov i, 1
loop:
  mov f, 0

  cmp i, 100
  jg done

  ; Is divisible by 3?
  mov n, i
  mov d, 3
  call mod
  cmp z, 0
  jne buzz
  add f, 1

  buzz:
  mov d, 5
  call mod
  cmp z, 0
  jne doprint
  add f, 2

doprint:
  cmp f, 0  ; if f is zero, it is not fizz nor buzz
  je print_num
  ; it can be either fizz or fizzbuzz, so we can
  ; print fizz already
  print 'fizz'
  cmp f, 3        ; is it buzz?
  jne loop_end    ; if it's not, jump to loop_end
  print 'buzz'
  jmp loop_end
print_num:
  print i

loop_end:
  print '\n'
  inc i
  jmp loop

done:
  end

; registers modified: t (temp), z (result)
; input registers: n = number, d = divisor
; result register: z
; equivalent to: number - (number / divisor) * divisor
mod:
  mov t, n
  div t, d
  mul t, d
  mov z, n
  sub z, t
  ret