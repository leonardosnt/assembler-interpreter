; kinda equivalent to this (in C):
; int is_atleast_fizz = 0;
; if (i % 3 == 0) {
;   printf("fizz");
;   is_atleast_fizz = 1;
; } // fall thru and check if is buzz
; if (i % 5 == 0) {
;   printf("buzz\n");
;   continue; // we can continue because it has already print either 'fizzbuzz' or 'buzz'
; }
; if (!is_atleast_fizz) {
;   printf("%d", i);
; }
; printf("\n");

mov i, 1
loop:
  mov x, 0
  cmp i, 100
  jg done

  ; Is divisible by 3?
  mov n, i
  mov d, 3
  call mod

  cmp z, 0
  jne check_buzz
  print 'fizz'
  inc x   ; mark as 'it is fizz or buzz'

  check_buzz:
  ; Is divisible by 5?
  mov d, 5
  call mod

  cmp z, 0
  jne maybe_neither
  print 'buzz'
  jmp loop_end

  maybe_neither:
    cmp x, 0  ; if x is zero, it is neither fizz nor buzz
    jne loop_end
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