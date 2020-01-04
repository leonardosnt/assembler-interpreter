mov b, 20
mov i, 1
loop:
  mov a, i
  call fib_iter
  print 'fib(', i, ') = ', z, '\n'
  inc i
  cmp i, b
  jle loop

end

fib_iter:
  push n
  push i ; loop
  push l ; last res (temp)
  push r ; res

  mov i, a
  mov n, 0
  mov r, 1

fib_iter_loop:
  cmp i, 0
  jle fib_iter_end
  dec i

  mov l, r
  add r, n
  mov n, l
  jmp fib_iter_loop

fib_iter_end:
  mov z, r

  pop r
  pop l
  pop i
  pop n
  ret