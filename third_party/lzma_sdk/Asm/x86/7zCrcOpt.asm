; 7zCrcOpt.asm -- CRC32 calculation : optimized version
; 2021-02-07 : Igor Pavlov : Public domain

include 7zAsm.asm

MY_ASM_START

rD   equ  r2
rN   equ  r7
rT   equ  r5

ifdef x64
    num_VAR     equ r8
    table_VAR   equ r9
else
  if (IS_CDECL gt 0)
    crc_OFFS    equ (REG_SIZE * 5)
    data_OFFS   equ (REG_SIZE + crc_OFFS)
    size_OFFS   equ (REG_SIZE + data_OFFS)
  else
    size_OFFS   equ (REG_SIZE * 5)
  endif
    table_OFFS  equ (REG_SIZE + size_OFFS)
    num_VAR     equ [r4 + size_OFFS]
    table_VAR   equ [r4 + table_OFFS]
endif

SRCDAT  equ  rD + rN * 1 + 4 *

CRC macro op:req, dest:req, src:req, t:req
    op      dest, DWORD PTR [rT + src * 4 + 0400h * t]
endm

CRC_XOR macro dest:req, src:req, t:req
    CRC xor, dest, src, t
endm

CRC_MOV macro dest:req, src:req, t:req
    CRC mov, dest, src, t
endm

CRC1b macro
    movzx   x6, BYTE PTR [rD]
    inc     rD
    movzx   x3, x0_L
    xor     x6, x3
    shr     x0, 8
    CRC     xor, x0, r6, 0
    dec     rN
endm

MY_PROLOG macro crc_end:req

    ifdef x64
      if  (IS_LINUX gt 0)
        MY_PUSH_2_REGS
        mov     x0, REG_ABI_PARAM_0_x   ; x0 = x7
        mov     rT, REG_ABI_PARAM_3     ; r5 = r1
        mov     rN, REG_ABI_PARAM_2     ; r7 = r2
        mov     rD, REG_ABI_PARAM_1     ; r2 = r6
      else
        MY_PUSH_4_REGS
        mov     x0, REG_ABI_PARAM_0_x   ; x0 = x1
        mov     rT, REG_ABI_PARAM_3     ; r5 = r9
        mov     rN, REG_ABI_PARAM_2     ; r7 = r8
        ; mov     rD, REG_ABI_PARAM_1     ; r2 = r2
      endif
    else
        MY_PUSH_4_REGS
      if  (IS_CDECL gt 0)
        mov     x0, [r4 + crc_OFFS]
        mov     rD, [r4 + data_OFFS]
      else
        mov     x0, REG_ABI_PARAM_0_x
      endif
        mov     rN, num_VAR
        mov     rT, table_VAR
    endif
    
    test    rN, rN
    jz      crc_end
  @@:
    test    rD, 7
    jz      @F
    CRC1b
    jnz     @B
  @@:
    cmp     rN, 16
    jb      crc_end
    add     rN, rD
    mov     num_VAR, rN
    sub     rN, 8
    and     rN, NOT 7
    sub     rD, rN
    xor     x0, [SRCDAT 0]
endm

MY_EPILOG macro crc_end:req
    xor     x0, [SRCDAT 0]
    mov     rD, rN
    mov     rN, num_VAR
    sub     rN, rD
  crc_end:
    test    rN, rN
    jz      @F
    CRC1b
    jmp     crc_end
  @@:
      if (IS_X64 gt 0) and (IS_LINUX gt 0)
        MY_POP_2_REGS
      else
        MY_POP_4_REGS
      endif
endm

MY_PROC CrcUpdateT8, 4
    MY_PROLOG crc_end_8
    mov     x1, [SRCDAT 1]
    align 16
  main_loop_8:
    mov     x6, [SRCDAT 2]
    movzx   x3, x1_L
    CRC_XOR x6, r3, 3
    movzx   x3, x1_H
    CRC_XOR x6, r3, 2
    shr     x1, 16
    movzx   x3, x1_L
    movzx   x1, x1_H
    CRC_XOR x6, r3, 1
    movzx   x3, x0_L
    CRC_XOR x6, r1, 0

    mov     x1, [SRCDAT 3]
    CRC_XOR x6, r3, 7
    movzx   x3, x0_H
    shr     x0, 16
    CRC_XOR x6, r3, 6
    movzx   x3, x0_L
    CRC_XOR x6, r3, 5
    movzx   x3, x0_H
    CRC_MOV x0, r3, 4
    xor     x0, x6
    add     rD, 8
    jnz     main_loop_8

    MY_EPILOG crc_end_8
MY_ENDP

MY_PROC CrcUpdateT4, 4
    MY_PROLOG crc_end_4
    align 16
  main_loop_4:
    movzx   x1, x0_L
    movzx   x3, x0_H
    shr     x0, 16
    movzx   x6, x0_H
    and     x0, 0FFh
    CRC_MOV x1, r1, 3
    xor     x1, [SRCDAT 1]
    CRC_XOR x1, r3, 2
    CRC_XOR x1, r6, 0
    CRC_XOR x1, r0, 1
 
    movzx   x0, x1_L
    movzx   x3, x1_H
    shr     x1, 16
    movzx   x6, x1_H
    and     x1, 0FFh
    CRC_MOV x0, r0, 3
    xor     x0, [SRCDAT 2]
    CRC_XOR x0, r3, 2
    CRC_XOR x0, r6, 0
    CRC_XOR x0, r1, 1
    add     rD, 8
    jnz     main_loop_4

    MY_EPILOG crc_end_4
MY_ENDP

end
