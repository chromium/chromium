; Copyright 2009 The Chromium Authors
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

; This file is just a convenient place for experimenting with x86 encodings.

BITS 32

; sldt to detect which processor we are running on.
sldt eax
sidt [esp]
sidt [esp+2]

lea eax, [fs:0]

mov eax, [fs:0x18]

mov ebx, 0x1234567
mov eax, 0x1234567

rdtsc

push eax
pop eax

mov eax, [ecx]
mov eax, [esp+4]
mov ebx, [esp+4]

lock xadd [eax], eax
lock xadd [ecx], ecx
lock xadd [ecx], eax

jmp eax
jmp edx

lodsd

rep stosb

rep movsb

mov eax, ebx
mov edx, edx

mov eax, eax

stosd

add eax, eax
add edi, ecx

and eax, 0x0000ffff
and ecx, 0x0000ffff
and edx, 0x0000ffff

add edi, 0x12345
add eax, 0x12345
add ecx, 0x12345

push 0x12
push BYTE 0x12

mov eax, [ebp+8]

mov eax, 0x1234
mov [fs:0], eax

call 0x1234

call eax
call ecx

add ebx, BYTE 3
or ecx, 0xffff
or eax, 0xffff

mov eax, [esp+24]

movsd
movsb

jmp blah
blah:
jmp blah

cmp eax, 0x1234567
cmp ecx, 0x1234567
je NEAR blah2
jo NEAR blah2
blah2:

add esp, 12
add esp, BYTE 12
sub esp, BYTE 12

cmp eax, 12
cmp ecx, BYTE 12

cmp WORD [esp+6], 0x6666

push DWORD [edi-4]
push DWORD [edi-8]
push DWORD [edi-12]
push DWORD [edi-16]
push DWORD [edi-20]

x:
loop x

mov edx, [fs:0x4]

cmp ecx, ecx
cmp ecx, ebx
cmp ebx, ebx

mov eax,[dword fs:0x24]
mov eax,[fs:0x24]

mov ecx,[dword fs:0x24]
mov ecx,[fs:0x24]

mov eax, [ebx+12]
mov ebx, [ebx+12]

cmovo eax, eax

mov eax, eax

xchg eax, ebx
xchg ebx, ecx
xchg ebx, [esp+4]
