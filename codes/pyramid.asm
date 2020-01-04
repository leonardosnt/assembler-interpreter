mov f, 50
print 'Printing a pyramid with ', f, ' floors!', '\n'
print '\n'
call print_pyramid
print '\n'
end

; f = num 'floors'
print_pyramid:
  ; for 0 .. num floors
  ; i = loop index
  mov i, 0
  floor_loop:
    cmp i, f
    jge floor_loop_end

    ; p = padding for this floor
    ; padding = num_floors - 1 - i
    mov p, f
    sub p, 1
    sub p, i

    ; t = num tiles
    mov t, i
    mul t, 2
    add t, 1

    call print_padding
    call print_tiles
    print '\n'

    inc i
    jmp floor_loop
  floor_loop_end:
    ret

; print spaces...
; reg p = num spaces
; uses j for loop index
print_padding:
  mov k, 0
print_padding_loop:
  cmp k, p
  jge print_padding_done
  print ' '
  inc k
  jmp print_padding_loop

print_padding_done:
  ret

; print #...
; reg t = num tiles
; uses k for loop index
print_tiles:
  mov k, 0
print_tiles_loop:
  cmp k, t
  jge print_tiles_done
  print '1'
  inc k
  jmp print_tiles_loop

print_tiles_done:
  ret