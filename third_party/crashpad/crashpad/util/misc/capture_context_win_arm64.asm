; Copyright 2019 The Crashpad Authors
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

  EXPORT |?CaptureContext@crashpad@@YAXPEAU_CONTEXT@@@Z|
  AREA |.text|, CODE
|?CaptureContext@crashpad@@YAXPEAU_CONTEXT@@@Z| PROC
  ; Save general purpose registers in context.regs[i].
  ; The original x0 can't be recovered.
  stp x0, x1, [x0, #0x008]
  stp x2, x3, [x0, #0x018]
  stp x4, x5, [x0, #0x028]
  stp x6, x7, [x0, #0x038]
  stp x8, x9, [x0, #0x048]
  stp x10, x11, [x0, #0x058]
  stp x12, x13, [x0, #0x068]
  stp x14, x15, [x0, #0x078]
  stp x16, x17, [x0, #0x088]
  stp x18, x19, [x0, #0x098]
  stp x20, x21, [x0, #0x0a8]
  stp x22, x23, [x0, #0x0b8]
  stp x24, x25, [x0, #0x0c8]
  stp x26, x27, [x0, #0x0d8]
  stp x28, x29, [x0, #0x0e8]

  ; The original LR can't be recovered.
  str LR, [x0, #0x0f8]

  ; Use x1 as a scratch register.
  mov x1, SP
  str x1, [x0, #0x100] ; context.sp

  ; The link register holds the return address for this function.
  str LR, [x0, #0x108]  ; context.pc

  ; pstate should hold SPSR but NZCV are the only bits we know about.
  mrs x1, NZCV

  ; Enable Control flags, such as CONTEXT_ARM64, CONTEXT_CONTROL,
  ; CONTEXT_INTEGER
  ldr w1, =0x00400003

  ; Set ControlFlags /0x000/ and pstate /0x004/ at the same time.
  str x1, [x0, #0x000]

  ; Restore x1 from the saved context.
  ldr x1, [x0, #0x010]

  ; TODO(https://crashpad.chromium.org/bug/300): save floating-point registers

  ret
  ENDP

  END
