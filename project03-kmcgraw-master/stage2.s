func:
    movq $0x000000000040320b, %rax
    callq *%rax
    movq %rax, %rdi
    movq $0x0000000000402f7d, %rax
    callq *%rax

