phase2:
    movq $0x45a64aea, %rdi
    addq $0x8, %rsp
    ret

phase3:
    movq $0x5566e878, %rdi
    addq $0x8, %rsp
    ret

phase4:
    popq %rax
    nop
    ret
    movq %rax, %rdi
    ret

phase5:
    movq %rsp, %rax # 48 89 e0 c3 <getval_243+0x2>
    ret
    movq %rax, %rdi # 48 89 c7 c3 <setval_211+0x2>
    ret
    popq %rax       # 58 90 c3 <setval_294+0x2>
    nop             #      (nop)
    ret
    movl %eax, %edx # 89 c2 38 d2 c3 <setval_197+0x2>
    cmpb %dl, %dl   #      (functional nop)
    ret
    movl %edx, %ecx # 89 d1 38 c0 c3 <addval_223+0x2>
    cmpb %al, %al   #      (functional nop)
    ret
    movl %ecx, %esi # 89 ce 84 c9 c3 <setval_248+0x2>
    testb %cl, %cl  #      (functional nop)
    ret
    lea (%rdi,%rsi,1),%rax # <add_xy>
    ret
    movq %rax, %rdi # 48 89 c7 c3 <setval_211+0x2>
    ret
