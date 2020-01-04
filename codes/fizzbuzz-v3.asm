; if (i % 15 == 0)      printf("fizzbuzz\n");
; else if (i % 3 == 0)  printf("fizz\n");
; else if (i % 5 == 0)  printf("buzz\n");
; else                  printf("%d\n", i);

mov i, 1
loop:
  cmp i, 100
  jg done

  mov n, i

  ; if i % 15 ?
  mov d, 15
  call mod
  cmp z, 0
  je fizzbuzz

  ; if i % 3 ?
  mov d, 3
  call mod
  cmp z, 0
  je fizz

  ; if i % 5
  mov d, 5
  call mod
  cmp z, 0
  je buzz

  ; else
  print i
  jmp loop_end

fizzbuzz:
  print 'fizzbuzz'
  jmp loop_end
fizz:
  print 'fizz'
  jmp loop_end
buzz:
  print 'buzz'
  jmp loop_end
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