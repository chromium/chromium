; XzCrc64Opt.asm -- CRC64 calculation : optimized version
; 2021-02-06 : Igor Pavlov : Public domain

include 7zAsm.asm

MY_ASM_START

ifdef x64

rD      equ  r9
rN      equ  r10
rT      equ  r5
num_VAR equ  r8

SRCDAT4 equ  dword ptr [rD + rN * 1]
    
CRC_XOR macro dest:req, src:req, t:req
    xor     dest, QWORD PTR [rT + src * 8 + 0800h * t]
endm

CRC1b macro
    movzx   x6, BYTE PTR [rD]
    inc     rD
    movzx   x3, x0_L
    xor     x6, x3
    shr     r0, 8
    CRC_XOR r0, r6, 0
    dec     rN
endm

MY_PROLOG macro crc_end:req
  ifdef ABI_LINUX
    MY_PUSH_2_REGS
  else
    MY_PUSH_4_REGS
  endif
    mov     r0, REG_ABI_PARAM_0
    mov     rN, REG_ABI_PARAM_2
    mov     rT, REG_ABI_PARAM_3
    mov     rD, REG_ABI_PARAM_1
    test    rN, rN
    jz      crc_end
  @@:
    test    rD, 3
    jz      @F
    CRC1b
    jnz     @B
  @@:
    cmp     rN, 8
    jb      crc_end
    add     rN, rD
    mov     num_VAR, rN
    sub     rN, 4
    and     rN, NOT 3
    sub     rD, rN
    mov     x1, SRCDAT4
    xor     r0, r1
    add     rN, 4
endm

MY_EPILOG macro crc_end:req
    sub     rN, 4
    mov     x1, SRCDAT4
    xor     r0, r1
    mov     rD, rN
    mov     rN, num_VAR
    sub     rN, rD
  crc_end:
    test    rN, rN
    jz      @F
    CRC1b
    jmp     crc_end
  @@:
  ifdef ABI_LINUX
    MY_POP_2_REGS
  else
    MY_POP_4_REGS
  endif
endm

MY_PROC XzCrc64UpdateT4, 4
    MY_PROLOG crc_end_4
    align 16
  main_loop_4:
    mov     x1, SRCDAT4
    movzx   x2, x0_L
    movzx   x3, x0_H
    shr     r0, 16
    movzx   x6, x0_L
    movzx   x7, x0_H
    shr     r0, 16
    CRC_XOR r1, r2, 3
    CRC_XOR r0, r3, 2
    CRC_XOR r1, r6, 1
    CRC_XOR r0, r7, 0
    xor     r0, r1

    add     rD, 4
    jnz     main_loop_4

    MY_EPILOG crc_end_4
MY_ENDP

else
; x86 (32-bit)

rD      equ  r1
rN      equ  r7
rT      equ  r5

crc_OFFS  equ  (REG_SIZE * 5)

if (IS_CDECL gt 0) or (IS_LINUX gt 0)
    ; cdecl or (GNU fastcall) stack:
    ;   (UInt32 *) table
    ;   size_t     size
    ;   void *     data
    ;   (UInt64)   crc
    ;   ret-ip <-(r4)
    data_OFFS   equ  (8 + crc_OFFS)
    size_OFFS   equ  (REG_SIZE + data_OFFS)
    table_OFFS  equ  (REG_SIZE + size_OFFS)
    num_VAR     equ  [r4 + size_OFFS]
    table_VAR   equ  [r4 + table_OFFS]
else
    ; Windows fastcall:
    ;   r1 = data, r2 = size
    ; stack:
    ;   (UInt32 *) table
    ;   (UInt64)   crc
    ;   ret-ip <-(r4)
    table_OFFS  equ  (8 + crc_OFFS)
    table_VAR   equ  [r4 + table_OFFS]
    num_VAR     equ  table_VAR
endif

SRCDAT4 equ  dword ptr [rD + rN * 1]

CRC macro op0:req, op1:req, dest0:req, dest1:req, src:req, t:req
    op0     dest0, DWORD PTR [rT + src * 8 + 0800h * t]
    op1     dest1, DWORD PTR [rT + src * 8 + 0800h * t + 4]
endm

CRC_XOR macro dest0:req, dest1:req, src:req, t:req
    CRC xor, xor, dest0, dest1, src, t
endm


CRC1b macro
    movzx   x6, BYTE PTR [rD]
    inc     rD
    movzx   x3, x0_L
    xor     x6, x3
    shrd    r0, r2, 8
    shr     r2, 8
    CRC_XOR r0, r2, r6, 0
    dec     rN
endm

MY_PROLOG macro crc_end:req
    MY_PUSH_4_REGS

  if (IS_CDECL gt 0) or (IS_LINUX gt 0)
    proc_numParams = proc_numParams + 2 ; for ABI_LINUX
    mov     rN, [r4 + size_OFFS]
    mov     rD, [r4 + data_OFFS]
  else
    mov     rN, r2
  endif

    mov     x0, [r4 + crc_OFFS]
    mov     x2, [r4 + crc_OFFS + 4]
    mov     rT, table_VAR
    test    rN, rN
    jz      crc_end
  @@:
    test    rD, 3
    jz      @F
    CRC1b
    jnz     @B
  @@:
    cmp     rN, 8
    jb      crc_end
    add     rN, rD

    mov     num_VAR, rN

    sub     rN, 4
    and     rN, NOT 3
    sub     rD, rN
    xor     r0, SRCDAT4
    add     rN, 4
endm

MY_EPILOG macro crc_end:req
    sub     rN, 4
    xor     r0, SRCDAT4

    mov     rD, rN
    mov     rN, num_VAR
    sub     rN, rD
  crc_end:
    test    rN, rN
    jz      @F
    CRC1b
    jmp     crc_end
  @@:
    MY_POP_4_REGS
endm

MY_PROC XzCrc64UpdateT4, 5
    MY_PROLOG crc_end_4
    movzx   x6, x0_L
    align 16
  main_loop_4:
    mov     r3, SRCDAT4
    xor     r3, r2

    CRC xor, mov, r3, r2, r6, 3
    movzx   x6, x0_H
    shr     r0, 16
    CRC_XOR r3, r2, r6, 2

    movzx   x6, x0_L
    movzx   x0, x0_H
    CRC_XOR r3, r2, r6, 1
    CRC_XOR r3, r2, r0, 0
    movzx   x6, x3_L
    mov     r0, r3

    add     rD, 4
    jnz     main_loop_4

    MY_EPILOG crc_end_4
MY_ENDP

endif ; ! x64

end
