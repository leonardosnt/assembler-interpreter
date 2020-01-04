; call func  ; this works
jmp func     ; but this doesn't, ret will cause callstack underflow
end

func:
  msg 'hi'
  ret