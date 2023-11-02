// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "assembler.h"

int main(int argc, char** argv) {
  char buf[1024];

  CodeBuffer cb(buf);

  // Branching tests first so the offsets are not always adjusting in the
  // output diassembler when we add new tests.

  cb.spin();

  cb.call(EAX);
  cb.call(Operand(EAX));
  cb.call(Operand(EDX, 15));

  cb.fs(); cb.mov(EAX, Operand(3));
  cb.fs(); cb.mov(EDX, Operand(0x04));

  cb.lea(EAX, Operand(EAX));
  cb.lea(EAX, Operand(0x12345678));
  cb.lea(EAX, Operand(EBX, 0x12345678));
  cb.lea(EAX, Operand(EBX, ECX, SCALE_TIMES_2, 0x12345678));
  cb.lea(EAX, Operand(ECX, SCALE_TIMES_2, 0x12345678));
  cb.lea(EAX, Operand(EAX, SCALE_TIMES_2, 0));
  cb.lea(EAX, Operand(EBX, SCALE_TIMES_2, 0));
  cb.lea(EBP, Operand(EBP, SCALE_TIMES_2, 1));

  cb.lodsb();
  cb.lodsd();

  cb.mov(EAX, ECX);
  cb.mov(ESI, ESP);
  cb.mov(EAX, Operand(ESP, 0x20));
  cb.mov(EAX, Operand(EBP, 8));
  cb.mov_imm(ESP, 1);
  cb.mov_imm(EAX, 0x12345678);

  cb.pop(EBX);
  cb.pop(Operand(EBX));
  cb.pop(Operand(EBX, 0));
  cb.pop(Operand(EBX, 12));

  cb.push(EBX);
  cb.push(Operand(EBX));
  cb.push(Operand(EBX, 0));
  cb.push(Operand(EDI, -4));
  cb.push(Operand(EDI, -8));
  cb.push_imm(0x12);
  cb.push_imm(0x1234);
  cb.push(Operand(EBX, 12));
  cb.push(Operand(ESP, 0x1234));

  cb.ret();
  cb.ret(0);
  cb.ret(12);

  cb.stosb();
  cb.stosd();

  cb.sysenter();

  cb.which_cpu();
  cb.which_thread();

  cb.xchg(EAX, EAX);
  cb.xchg(EBX, EAX);
  cb.xchg(EAX, EBX);
  cb.xchg(ECX, ESP);
  cb.xchg(ECX, Operand(ESP));
  cb.xchg(ECX, Operand(ESP, 5));
  cb.xchg(ECX, Operand(EDX, 4));

  fwrite(buf, 1, cb.size(), stdout);

  return 0;
}
