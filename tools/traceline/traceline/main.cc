// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO
//  - Make capturing system call arguments optional and the number configurable.
//  - Lots of places depend on the ABI so that we can modify EAX or EDX, this
//    is safe, but these could be moved to be saved and restored anyway.
//  - Understand the loader better, and make some more meaningful hooks with
//    proper data collection and durations.  Right now it's just noise.
//  - Get the returned pointer from AllocateHeap.

#include <windows.h>

#include <stdio.h>

#include <map>
#include <string>

#include "assembler.h"
#include "logging.h"
#include "rdtsc.h"
#include "sidestep/mini_disassembler.h"
#include "sym_resolver.h"
#include "syscall_map.h"

namespace {

std::string JSONString(const std::string& str) {
  static const char hextable[] = "0123456789abcdef";
  std::string out;
  out.push_back('"');
  for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
    unsigned char c = static_cast<unsigned char>(*it);
    switch (c) {
      case '\\':
      case '"':
      case '\'':
        out.push_back('\\'); out.push_back(c);
        break;
      default:
        if (c < 20 || c >= 127) {
          out.push_back('\\'); out.push_back('x');
          out.push_back(hextable[c >> 4]); out.push_back(hextable[c & 0xf]);
        } else {
          // Unescaped.
          out.push_back(c);
        }
        break;
    }
  }
  out.push_back('"');
  return out;
}

}  // namespace

class Playground {
 public:
  static const int kPlaygroundSize = 64 * 1024 * 1024;

  // Encapsulate the configuration options to the playground.
  class Options {
   public:
    Options()
        : stack_unwind_depth_(0),
          log_heap_(false),
          log_lock_(false),
          vista_(false) { }


    // The maximum amount of frames we should unwind from the call stack.
    int stack_unwind_depth() { return stack_unwind_depth_; }
    void set_stack_unwind_depth(int depth) { stack_unwind_depth_ = depth; }

    // Whether we should log heap operations (alloc / free).
    bool log_heap() { return log_heap_; }
    void set_log_heap(bool x) { log_heap_ = x; }

    // Whether we should log lock (critical section) operations.
    bool log_lock() { return log_lock_; }
    void set_log_lock(bool x) { log_lock_ = x; }

    // Whether we are running on Vista.
    bool vista() { return vista_; }
    void set_vista(bool x) { vista_ = x; }

   private:
    int stack_unwind_depth_;
    bool log_heap_;
    bool log_lock_;
    bool vista_;
  };

  Playground(HANDLE proc, const Options& options)
      : proc_(proc),
        remote_addr_(NULL),
        resolver_("ntdll.dll"),
        options_(options) {
    // We copy the entire playground into the remote process, and we have
    // fields that we expect to be zero.  TODO this could be a lot better.
    memset(buf_, 0, sizeof(buf_));
  }

  void AllocateInRemote() {
    // Try to get something out of the way and easy to debug.
    static void* kPlaygroundAddr = reinterpret_cast<void*>(0x66660000);
    // Allocate our playground memory in the target process.  This is a big
    // slab of read/write/execute memory that we use for our code
    // instrumentation, and the memory for writing out our logging events.
    remote_addr_ = reinterpret_cast<char*>(
        VirtualAllocEx(proc_,
                       kPlaygroundAddr,
                       kPlaygroundSize,
                       MEM_COMMIT | MEM_RESERVE,
                       PAGE_EXECUTE_READWRITE));
    if (remote_addr_ == NULL || remote_addr_ != kPlaygroundAddr) {
      NOTREACHED("Falied to allocate playground: 0x%08x", remote_addr_);
    }
  }

  void CopyToRemote() {
    WriteProcessMemory(proc_,
                       remote_addr_,
                       buf_,
                       sizeof(buf_),
                       NULL);
  }

  void CopyFromRemote() {
    SIZE_T size = 0;
    ReadProcessMemory(proc_,
                      remote_addr_,
                      buf_,
                      sizeof(buf_),
                      &size);
  }

  enum EventRecordType {
    EVENT_TYPE_LDR              = 0,
    EVENT_TYPE_THREADBEGIN      = 1,
    EVENT_TYPE_THREADNAME       = 2,
    EVENT_TYPE_EXCEPTION        = 3,
    EVENT_TYPE_PROCESSEXIT      = 4,
    EVENT_TYPE_CREATETHREAD     = 5,
    EVENT_TYPE_THREADEXIT       = 6,
    EVENT_TYPE_ALLOCHEAP        = 7,
    EVENT_TYPE_FREEHEAP         = 8,
    EVENT_TYPE_SYSCALL          = 9,
    EVENT_TYPE_ENTER_CS         = 10,
    EVENT_TYPE_TRYENTER_CS      = 11,
    EVENT_TYPE_LEAVE_CS         = 12,
    EVENT_TYPE_APC              = 13
  };

  static const int kThreadNameBufSize = 64;
  static const int kLdrBufSize = 512;  // Looks like internal buffer is 512.

  static const int kCodeBlockSize               = 256;

  static const int kOffLdrCode                  = 0 * kCodeBlockSize;
  static const int kOffCreateThreadCode         = 1 * kCodeBlockSize;
  static const int kOffThreadCode               = 2 * kCodeBlockSize;
  static const int kOffExpCode                  = 3 * kCodeBlockSize;
  static const int kOffExitCode                 = 4 * kCodeBlockSize;
  static const int kOffThreadExitCode           = 5 * kCodeBlockSize;
  static const int kOffAllocHeapCode            = 6 * kCodeBlockSize;
  static const int kOffFreeHeapCode             = 7 * kCodeBlockSize;
  static const int kOffSyscallCode              = 8 * kCodeBlockSize;
  static const int kOffEnterCritSecCode         = 9 * kCodeBlockSize;
  static const int kOffTryEnterCritSecCode      = 10 * kCodeBlockSize;
  static const int kOffLeaveCritSecCode         = 11 * kCodeBlockSize;
  static const int kOffApcDispCode              = 12 * kCodeBlockSize;

  static const int kOffLogAreaPtr               = 4096;
  static const int kOffLogAreaData              = 4096 + 4;

  static const int kRecordHeaderSize = 8 + 4 + 4 + 4;

  // Given the address to the start of a function, patch the function to jump
  // to a given offset into the playground.  This function will try to take
  // advantage of hotpatch code, if the function is prefixed with 5 0x90 bytes.
  // Returns a std::string of any assembly instructions that must be relocated,
  // as they were overwritten during patching.
  std::string PatchPreamble(int func_addr, int playground_off) {
    sidestep::MiniDisassembler disas;
    int stub_addr = reinterpret_cast<int>(remote_addr_ + playground_off);

    std::string instrs;

    char buf[15];
    if (ReadProcessMemory(proc_,
                          reinterpret_cast<void*>(func_addr - 5),
                          buf,
                          sizeof(buf),
                          NULL) == 0) {
      NOTREACHED("ReadProcessMemory(0x%08x) failed: %d",
                 func_addr - 5, GetLastError());
    }

    // TODO(deanm): It seems in more recent updates the compiler is generating
    // complicated sequences for padding / alignment.  For example:
    // 00000000  8DA42400000000    lea esp,[esp+0x0]
    // 00000007  8D4900            lea ecx,[ecx+0x0]
    // is used for a 16 byte alignment.  We need a better way of handling this.
    if (memcmp(buf, "\x90\x90\x90\x90\x90", 5) == 0 ||
        memcmp(buf, "\x00\x8D\x64\x24\x00", 5) == 0 ||
        memcmp(buf, "\x00\x00\x8D\x49\x00", 5) == 0) {
      unsigned int instr_bytes = 0;

      // We might have a hotpatch no-op of mov edi, edi "\x8b\xff".  It is a
      // bit of a waste to relocate it, but it makes everything simpler.

      while (instr_bytes < 2) {
        if (disas.Disassemble(
            reinterpret_cast<unsigned char*>(buf + 5 + instr_bytes),
            &instr_bytes) != sidestep::IT_GENERIC) {
          NOTREACHED("Could not disassemble or relocate instruction.");
        }
        // We only read 10 bytes worth of instructions.
        CHECK(instr_bytes < 10);
      }

      instrs.assign(buf + 5, instr_bytes);

      // We have a hotpatch prefix of 5 nop bytes.  We can use this for our
      // long jump, and then overwrite the first 2 bytes to jump back to there.
      CodeBuffer patch(buf);
      int off = stub_addr - func_addr;
      patch.jmp_rel(off);
      patch.jmp_rel_short(-2 - 5);
    } else {
      // We need a full 5 bytes for the jump.
      unsigned int instr_bytes = 0;
      while (instr_bytes < 5) {
        if (disas.Disassemble(
            reinterpret_cast<unsigned char*>(buf + 5 + instr_bytes),
            &instr_bytes) != sidestep::IT_GENERIC) {
          NOTREACHED("Could not disassemble or relocate instruction.");
        }
        // We only read 10 bytes worth of instructions.
        CHECK(instr_bytes < 10);
      }

      instrs.assign(buf + 5, instr_bytes);

      // Overwrite the first 5 bytes with a relative jump to our stub.
      CodeBuffer patch(buf + 5);
      int off = stub_addr - (func_addr + 5);
      patch.jmp_rel(off);
    }

    // Write back the bytes, we are really probably writing more back than we
    // need to, but it shouldn't really matter.
    if (WriteProcessMemory(proc_,
                           reinterpret_cast<void*>(func_addr - 5),
                           buf,
                           sizeof(buf),
                           NULL) == 0) {
      NOTREACHED("WriteProcessMemory(0x%08x) failed: %d",
                 func_addr - 5, GetLastError());
    }

    return instrs;
  }

  std::string PatchPreamble(const char* func_name, int playground_off) {
    return PatchPreamble(
        reinterpret_cast<int>(resolver_.Resolve(func_name)), playground_off);
  }

  // Restore any instructions that needed to be moved to make space for our
  // patch and jump back to the original code.
  void ResumeOriginalFunction(const char* func_name,
                              const std::string& moved_instructions,
                              int stub_offset,
                              CodeBuffer* cb) {
    cb->emit_bytes(moved_instructions);
    int off = resolver_.Resolve(func_name) +
              moved_instructions.size() -
              (remote_addr_ + stub_offset + cb->size() + 5);
    cb->jmp_rel(off);
  }

  // Makes a call to NtQueryPerformanceCounter, writing the timestamp to the
  // buffer pointed to by EDI.  EDI it not incremented.  EAX is not preserved.
  void AssembleQueryPerformanceCounter(CodeBuffer* cb) {
    // Make a call to NtQueryPerformanceCounter and write the result into
    // the log area.  The buffer we write to should be aligned, but we should
    // garantee that anyway for the logging area for performance.
    cb->push_imm(0);       // PerformanceFrequency
    cb->push(EDI);         // PerformanceCounter
    cb->mov_imm(EAX, reinterpret_cast<int>(
        resolver_.Resolve("ntdll!NtQueryPerformanceCounter")));
    cb->call(EAX);
  }

  // This is the common log setup routine.  It will allocate a new log entry,
  // and write out the common log header to the event entry.  The header is:
  // is [ 64bit QPC ] [ 32bit cpu id ] [ 32bit thread id ] [ 32bit rec id ]
  // EDI will be left pointing to the log entry, with |space| bytes left for
  // type specific data.  All other registers should not be clobbered.
  void AssembleHeaderCode(CodeBuffer* cb, EventRecordType rt, int space) {
    cb->push(EAX);
    cb->push(EDX);
    cb->push(ECX);
    cb->push(ESI);

    int unwind_depth = options_.stack_unwind_depth();

    // Load EDI with the number of bytes we want for our log entry, this will
    // be used in the atomic increment to allocate the log entry.
    cb->mov_imm(EDI, kRecordHeaderSize + (unwind_depth * 4) + space);
    // Do the increment and have EDI point to our log entry buffer space.
    cb->mov_imm(EDX, reinterpret_cast<int>(remote_addr_ + kOffLogAreaPtr));
    cb->inc_atomic(EDX, EDI);
    // EDI is the buffer offset, make it a pointer to the record entry.
    cb->add_imm(EDI, reinterpret_cast<int>(remote_addr_ + kOffLogAreaData));

    AssembleQueryPerformanceCounter(cb);
    cb->add_imm(EDI, 8);

    cb->which_cpu();
    cb->stosd();

    cb->which_thread();
    cb->stosd();

    // Stack unwinding, follow EBP to the maximum number of frames, and make
    // sure that it stays on the stack (between ESP and TEB.StackBase).
    if (unwind_depth > 0) {
      cb->mov_imm(ECX, unwind_depth);
      cb->fs(); cb->mov(EDX, Operand(0x04));  // get TEB.StackBase

      // Start at EBP.
      cb->mov(EAX, EBP);

      Label unwind_loop, bail;
      cb->bind(&unwind_loop);

      // Bail if (EAX < ESP) (below the stack)
      cb->cmp(EAX, ESP);
      cb->jcc(below, &bail);
      // Bail if (EAX >= EDX) (above the stack)
      cb->cmp(EAX, EDX);
      cb->jcc(above_equal, &bail);

      // We have a valid stack pointer, it should point to something like:
      //   [ saved frame pointer ] [ return address ] [ arguments ... ]
      cb->mov(ESI, EAX);
      cb->lodsd();  // Get the new stack pointer to follow in EAX
      cb->movsd();  // Copy the return address to the log area.

      cb->loop(&unwind_loop);

      cb->bind(&bail);
      // If we did managed to unwind to the max, fill the rest with 0 (really
      // we just want to inc EDI to the end, and this is an easy way).
      cb->mov_imm(EAX, 0);  // TODO use an xor
      cb->rep(); cb->stosd();
    }

    // Store the type for this record entry.
    cb->mov_imm(EAX, rt);
    cb->stosd();

    cb->pop(ESI);
    cb->pop(ECX);
    cb->pop(EDX);
    cb->pop(EAX);
  }

  void PatchLoader() {
    static const EventRecordType kRecordType =  EVENT_TYPE_LDR;
    static const char* kFuncName = "ntdll!DebugPrint";
    static const int kStubOffset = kOffLdrCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    // Set ShowSnaps to one to get the print routines to be called.
    char enabled = 1;
    WriteProcessMemory(
        proc_, resolver_.Resolve("ntdll!ShowSnaps"), &enabled, 1, NULL);

    CodeBuffer cb(buf_ + kStubOffset);

    cb.pop(EDX);  // return address
    cb.pop(EAX);  // First param in eax
    cb.push(ESI);
    cb.push(EDI);
    cb.push(EDX);

    cb.mov(ESI, EAX);  // ESI points at the string structure.

    // We used to do variable length based on the length supplied in the str
    // structure, but it's easier (and sloppier) to just copy a fixed amount.
    AssembleHeaderCode(&cb, kRecordType, kLdrBufSize);

    cb.lodsd();        // Load the character count
    cb.lodsd();        // Load the char*
    cb.mov(ESI, EAX);
    cb.mov_imm(ECX, kLdrBufSize / 4);  // load the char count as the rep count
    cb.rep(); cb.movsb();  // Copy the string to the logging buffer

    // Return
    cb.pop(EDX);
    cb.pop(EDI);
    cb.pop(ESI);
    cb.pop(ECX);  // don't care
    cb.pop(ECX);  // don't care
    cb.jmp(EDX);
  }

  void PatchCreateThread() {
    static const EventRecordType kRecordType =  EVENT_TYPE_CREATETHREAD;
    static const char* kFuncName =
      options_.vista() ? "ntdll!NtCreateThreadEx" : "ntdll!NtCreateThread";
    static const int kStubOffset = kOffCreateThreadCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    CodeBuffer cb(buf_ + kStubOffset);

    cb.push(EDI);
    cb.push(ESI);

    AssembleHeaderCode(&cb, kRecordType, 8);

    cb.mov(EAX, Operand(ESP, 0x18 + 8));

    // Super ugly hack.  To coorrelate between creating a thread and the new
    // thread running, we stash something to identify the creating event when
    // we log the created event.  We just use a pointer to the event log data
    // since this will be unique and can tie the two events together.  We pass
    // it by writing into the context structure, so it will be passed in ESI.
    cb.add_imm(EAX, 0xa0);
    cb.push(EDI);
    cb.mov(EDI, EAX);
    cb.pop(EAX);
    cb.push(EAX);
    cb.stosd();

    // Get and save CONTEXT.Eip
    cb.mov(ESI, EDI);
    cb.add_imm(ESI, 20);
    cb.pop(EDI);
    cb.mov(EAX, EDI);
    cb.stosd();  // Record the event identifier to tie together the events.
    cb.movsd();  // write Eip to the log event

    cb.pop(ESI);
    cb.pop(EDI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }

  void PatchThreadBegin() {
    static const EventRecordType kRecordType =  EVENT_TYPE_THREADBEGIN;
    static const char* kFuncName = "ntdll!CsrNewThread";
    static const int kStubOffset = kOffThreadCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    CodeBuffer cb(buf_ + kStubOffset);

    cb.push(EDI);

    AssembleHeaderCode(&cb, kRecordType, 8);

    cb.mov(EAX, ESI);  // We stashed the creator's eventid in the context ESI.
    cb.stosd();

    // TODO(deanm): The pointer is going to point into the CRT or something,
    // should we dig deeper to get more information about the real entry?
    cb.mov(EAX, Operand(EBP, 0x8));
    cb.stosd();
    cb.pop(EDI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }

  void PatchThreadBeginVista() {
    static const EventRecordType kRecordType =  EVENT_TYPE_THREADBEGIN;
    static const char* kFuncName = "ntdll!_RtlUserThreadStart";
    static const int kStubOffset = kOffThreadCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    CodeBuffer cb(buf_ + kStubOffset);

    cb.push(EDI);

    AssembleHeaderCode(&cb, kRecordType, 8);

    cb.mov(EAX, ESI);  // We stashed the creator's eventid in the context ESI.
    cb.stosd();

    // TODO(deanm): The pointer is going to point into the CRT or something,
    // should we dig deeper to get more information about the real entry?
    //cb.mov(EAX, Operand(EBP, 0x8));
    cb.mov_imm(EAX, 0);
    cb.stosd();
    cb.pop(EDI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }

  // Intercept exception dispatching so we can catch when threads set a thread
  // name (which is an exception with a special code).  TODO it could be
  // useful to log all exceptions.
  void PatchSetThreadName() {
    static const EventRecordType kRecordType =  EVENT_TYPE_THREADNAME;
    static const char* kFuncName = "ntdll!RtlDispatchException";
    static const int kStubOffset = kOffExpCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    CodeBuffer cb(buf_ + kStubOffset);

    cb.pop(EDX);  // return address
    cb.pop(EAX);  // ExceptionRecord
    cb.push(EAX);
    cb.push(EDX);

    cb.push(ESI);

    cb.mov(ESI, EAX);
    cb.lodsd();

    Label bail;
    // exception code
    cb.cmp_imm(EAX, 0x406D1388);
    cb.jcc(not_equal, &bail);

    cb.push(EDI);

    AssembleHeaderCode(&cb, kRecordType, kThreadNameBufSize);

    // Fetch the second parameter.
    for (int i = 0; i < 6; ++i) {
      cb.lodsd();
    }

    // TODO This is sloppy and we could run into unmapped memory...
    cb.mov(ESI, EAX);
    cb.mov_imm(ECX, kThreadNameBufSize / 4);
    cb.rep(); cb.movsd();

    cb.pop(EDI);

    cb.bind(&bail);
    cb.pop(ESI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }


  void PatchThreadExit() {
    static const EventRecordType kRecordType =  EVENT_TYPE_THREADEXIT;
    static const char* kFuncName = "ntdll!LdrShutdownThread";
    static const int kStubOffset = kOffThreadExitCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);
    CodeBuffer cb(buf_ + kStubOffset);

    cb.push(EDI);
    AssembleHeaderCode(&cb, kRecordType, 0);
    cb.pop(EDI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }

  void PatchAllocateHeap() {
    static const EventRecordType kRecordType =  EVENT_TYPE_ALLOCHEAP;
    static const char* kFuncName = "ntdll!RtlAllocateHeap";
    static const int kStubOffset = kOffAllocHeapCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);
    CodeBuffer cb(buf_ + kStubOffset);

    cb.push(EDI);
    cb.push(ESI);

    AssembleHeaderCode(&cb, kRecordType, 12);

    cb.mov(ESI, ESP);
    cb.add_imm(ESI, 12);  // Skip over our saved and the return address
    cb.movsd(); cb.movsd(); cb.movsd();  // Copy the 3 parameters

    cb.pop(ESI);
    cb.pop(EDI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }

  void PatchFreeHeap() {
    static const EventRecordType kRecordType =  EVENT_TYPE_FREEHEAP;
    static const char* kFuncName = "ntdll!RtlFreeHeap";
    static const int kStubOffset = kOffFreeHeapCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);
    CodeBuffer cb(buf_ + kStubOffset);

    cb.push(EDI);
    cb.push(ESI);

    AssembleHeaderCode(&cb, kRecordType, 12);

    cb.mov(ESI, ESP);
    cb.add_imm(ESI, 12);  // Skip over our saved and the return address
    cb.movsd(); cb.movsd(); cb.movsd();  // Copy the 3 parameters

    cb.pop(ESI);
    cb.pop(EDI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }

  // Don't even bother going back to the original code, just implement our
  // own KiFastSystemCall.  The original looks like:
  //   .text:7C90EB8B                 mov     edx, esp
  //   .text:7C90EB8D                 sysenter
  //   .text:7C90EB8F                 nop
  //   .text:7C90EB90                 nop
  //   .text:7C90EB91                 nop
  //   .text:7C90EB92                 nop
  //   .text:7C90EB93                 nop
  void PatchSyscall() {
    static const EventRecordType kRecordType =  EVENT_TYPE_SYSCALL;
    static const char* kFuncName = "ntdll!KiFastSystemCall";
    static const int kStubOffset = kOffSyscallCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    {
      CodeBuffer cb(buf_ + kStubOffset);

      Label skip;

      // Skip 0xa5 which is QueryPerformanceCounter, to make sure we don't log
      // our own logging's QPC.  Disabled for now, using ret addr check...
      // cb.cmp_imm(EAX, 0xa5);
      // cb.jcc(equal, &skip);

      // Check if the return address is from 0x6666 (our code region).
      // 66817C24066666    cmp word [esp+0x6],0x6666
      cb.emit(0x66); cb.emit(0x81); cb.emit(0x7C);
      cb.emit(0x24); cb.emit(0x06); cb.emit(0x66); cb.emit(0x66);
      cb.jcc(equal, &skip);

      // This is all a bit shit.  Originally I thought I could store some state
      // on the stack above ESP, however, it seems that when APCs, etc are
      // queued, they will use the stack above ESP.  Well, not above ESP, above
      // what was passed in as EDX into the systemcall, not matter if ESP was
      // different than this :(.  So we need to store our state in the event
      // log record, and then we stick a pointer to that over a ret addr...

      // Our stack starts like:
      //  [ ret addr ] [ ret addr 2 ] [ arguments ]
      // We will update it to look like
      //  [ ret stub addr ] [ event entry ptr ] [ arguments ]

      cb.push(EDI);  // save EDI since we're using it
      AssembleHeaderCode(&cb, kRecordType, 16 + 16 + 8);
      cb.mov(EDX, EAX);  // Save EAX...
      cb.stosd();  // eax is the syscall number
      cb.pop(EAX);
      cb.stosd();  // store the saved EDI
      cb.pop(EAX);
      cb.stosd();  // store the real return address
      cb.pop(EAX);
      cb.stosd();  // store the real (secondary) return address;

      cb.push(ESI);
      cb.mov(ESI, ESP);
      cb.lodsd();
      cb.movsd();  // argument 1
      cb.movsd();  // argument 2
      cb.movsd();  // argument 3
      cb.pop(ESI);

      cb.push(EDI);  // store our event ptr over the secondary ret addr.
      cb.push_imm(reinterpret_cast<int>(remote_addr_ + kOffSyscallCode + 200));
      cb.mov(EAX, EDX);  // restore EAX

      cb.bind(&skip);
      cb.mov(EDX, ESP);
      cb.sysenter();

      if (cb.size() > 200) {
        NOTREACHED("code too big: %d", cb.size());
      }
    }

    {
      CodeBuffer cb(buf_ + kStubOffset + 200);

      // TODO share the QPC code, this is a copy and paste...

      cb.pop(EDI);  // get the log area

      cb.stosd();   // Log the system call return value.

      // QPC will clobber EAX, and it's very important to save it since it
      // is the return value from the system call.  TODO validate if there is
      // anything else we need to save...
      cb.push(EAX);
      AssembleQueryPerformanceCounter(&cb);
      cb.pop(EAX);

      // We need to:
      //  - Restore the original "seconary" return address
      //  - Restore the original value of the EDI register
      //  - Jump control flow to the original return address
      // All 3 of these values are stored in the log record...
      // [ syscall num ] [ saved edi ] [ real rets ] [ args ] [ retval ] [ ts ]
      //                   currently edi points here     ----^

      cb.push(Operand(EDI, -4 - 16));   // push the real 2nd ret
      cb.push(Operand(EDI, -8 - 16));   // push the real ret
      cb.push(Operand(EDI, -12 - 16));  // push the saved EDI

      cb.pop(EDI);  // restore EDI that was saved in the record
      cb.ret();     // jmp back to the real ret ...

      if (cb.size() > 56) {
        NOTREACHED("ug");
      }
    }
  }

  // Patch lock (criticial section) holding.
  void PatchEnterCriticalSection() {
    static const EventRecordType kRecordType =  EVENT_TYPE_ENTER_CS;
    static const char* kFuncName = "ntdll!RtlEnterCriticalSection";
    static const int kStubOffset = kOffEnterCritSecCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    // We just want to capture the return address and original argument, so
    // we know when EnterCriticalSection returned, we don't want to know when
    // it entered because it could sit waiting.  We want to know when the lock
    // actually started being held.  The compiler will sometimes generated code
    // that overwrites arguments, so we'll keep a copy of the argument just in
    // case code like this is ever generated in the future.  TODO is it enough
    // to just assume a LPCRITICAL_SECTION uniquely identifies the lock, or
    // can the same lock have multiple different copies, I would assume not.
    {
      CodeBuffer cb(buf_ + kStubOffset);

      // Set up an additional frame so that we capture the return.
      // TODO use memory instructions instead of using registers.
      cb.pop(EAX);  // return address
      cb.pop(EDX);  // first argument (critical section pointer)

      cb.push(EDX);
      cb.push(EAX);
      cb.push(EDX);
      cb.push_imm(
          reinterpret_cast<int>(remote_addr_ + kStubOffset + 40));

      ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
      CHECK(cb.size() < 40);
    }

    {
      CodeBuffer cb(buf_ + kStubOffset + 40);

      cb.push(ESI);
      cb.mov(ESI, ESP);
      cb.push(EAX);
      cb.push(EDI);

      AssembleHeaderCode(&cb, kRecordType, 4);

      cb.lodsd();  // Skip over our saved ESI
      cb.lodsd();  // Skip over the return address
      cb.movsd();  // Write the CRITICAL_SECTION* to the event record.

      cb.pop(EDI);
      cb.pop(EAX);
      cb.pop(ESI);

      cb.ret(0x04);
    }
  }

  void PatchTryEnterCriticalSection() {
    static const EventRecordType kRecordType =  EVENT_TYPE_TRYENTER_CS;
    static const char* kFuncName = "ntdll!RtlTryEnterCriticalSection";
    static const int kStubOffset = kOffTryEnterCritSecCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    {
      CodeBuffer cb(buf_ + kStubOffset);

      // Set up an additional frame so that we capture the return.
      // TODO use memory instructions instead of using registers.
      cb.pop(EAX);  // return address
      cb.pop(EDX);  // first argument (critical section pointer)

      cb.push(EDX);
      cb.push(EAX);
      cb.push(EDX);
      cb.push_imm(reinterpret_cast<int>(remote_addr_ + kStubOffset + 40));

      ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
      CHECK(cb.size() < 40);
    }

    {
      CodeBuffer cb(buf_ + kStubOffset + 40);

      cb.push(ESI);
      cb.mov(ESI, ESP);
      cb.push(EDI);

      cb.push(EAX);

      AssembleHeaderCode(&cb, kRecordType, 8);

      cb.lodsd();  // Skip over our saved ESI
      cb.lodsd();  // Skip over the return address
      cb.movsd();  // Write the CRITICAL_SECTION* to the event record.

      cb.pop(EAX);
      cb.stosd();  // Write the return value to the event record.

      cb.pop(EDI);
      cb.pop(ESI);

      cb.ret(0x04);
    }
  }

  void PatchLeaveCriticalSection() {
    static const EventRecordType kRecordType =  EVENT_TYPE_LEAVE_CS;
    static const char* kFuncName = "ntdll!RtlLeaveCriticalSection";
    static const int kStubOffset = kOffLeaveCritSecCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);
    CodeBuffer cb(buf_ + kStubOffset);

    // TODO use memory instructions instead of using registers.
    cb.pop(EDX);  // return address
    cb.pop(EAX);  // first argument (critical section pointer)
    cb.push(EAX);
    cb.push(EDX);

    cb.push(EDI);
    AssembleHeaderCode(&cb, kRecordType, 4);
    cb.stosd();  // Write the CRITICAL_SECTION* to the event record.
    cb.pop(EDI);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }

  // Patch APC dispatching.  This is a bit hacky, since the return to kernel
  // mode is done with NtContinue, we have to shim in a stub return address to
  // catch when the callback is finished.  It is probably a bit fragile.
  void PatchApcDispatcher() {
    static const EventRecordType kRecordType =  EVENT_TYPE_APC;
    static const char* kFuncName = "ntdll!KiUserApcDispatcher";
    static const int kStubOffset = kOffApcDispCode;

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);

    {
      CodeBuffer cb(buf_ + kStubOffset);

      // We don't really need to preserve these since we're the first thing
      // executing from the kernel dispatch, but yeah, it is good practice.
      cb.push(EDI);
      cb.push(EAX);

      AssembleHeaderCode(&cb, kRecordType, 4 + 4 + 8);

      cb.mov_imm(EAX, reinterpret_cast<int>(remote_addr_ + kStubOffset + 140));
      cb.xchg(EAX, Operand(ESP, 8));  // Swap the callback address with ours.
      cb.stosd();  // Store the original callback function address.

      // TODO for now we're lazy and depend that ESI will be preserved, and we
      // use it to store the pointer into our log record.  EDI isn't preserved.
      cb.mov(ESI, EDI);

      cb.pop(EAX);
      cb.pop(EDI);

      ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);

      CHECK(cb.size() < 140);
    }
    {
      CodeBuffer cb(buf_ + kStubOffset + 140);

      // This is our shim, we need to call the original callback function, then
      // we can catch the return and log when it was completed.
      cb.pop(EAX);  // The real return address, safe to use EAX w/ the ABI?
      cb.push(EDI);

      cb.mov(EDI, ESI);
      cb.stosd();  // Store the real return address, we'll need it.

      cb.add_imm(ESI, -4);
      cb.lodsd();   // Load the real callback address.

      cb.mov(ESI, EDI);
      cb.pop(EDI);

      cb.call(EAX);  // Call the original callback address.

      cb.push(EAX);
      cb.push(EDI);

      cb.mov(EDI, ESI);
      AssembleQueryPerformanceCounter(&cb);

      cb.pop(EDI);
      cb.pop(EAX);

      cb.push(Operand(ESI, -4));  // Push the real return address.
      cb.ret();  // Return back to the APC Dispatcher.

      CHECK(cb.size() < 50);
    }
  }

  // We need to hook into process shutdown for two reasons.  Most importantly,
  // we need to copy the playground back from the process before the address
  // space goes away.  We could avoid this with shared memory, however, there
  // is a reason two.  In order to capture symbols for all of the libraries
  // loaded into arbitrary applications, on shutdown we do an instrusive load
  // of symbols into the traced process.
  //
  // ntdll!LdrShutdownProcess
  //  - NtSetEvent(event, 0);
  //  - NtWaitForSingleObject(event, FALSE, NULL);
  //  - jmp back
  void PatchExit(HANDLE exiting, HANDLE exited) {
    static const EventRecordType kRecordType =  EVENT_TYPE_PROCESSEXIT;
    static const char* kFuncName = "ntdll!LdrShutdownProcess";
    static const int kStubOffset = kOffExitCode;

    HANDLE rexiting, rexited;
    if (!DuplicateHandle(::GetCurrentProcess(),
                         exiting,
                         proc_,
                         &rexiting,
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      NOTREACHED("");
    }
    if (!DuplicateHandle(::GetCurrentProcess(),
                         exited,
                         proc_,
                         &rexited,
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      NOTREACHED("");
    }

    std::string moved_instructions = PatchPreamble(kFuncName, kStubOffset);
    CodeBuffer cb(buf_ + kStubOffset);

    cb.push(EDI);
    AssembleHeaderCode(&cb, kRecordType, 0);
    cb.pop(EDI);

    // NtSetEvent(exiting, 0);
    cb.push_imm(0);
    cb.push_imm(reinterpret_cast<int>(rexiting));
    cb.mov_imm(EAX, reinterpret_cast<int>(
        resolver_.Resolve("ntdll!NtSetEvent")));
    cb.call(EAX);

    // NtWaitForSingleObject(exited, FALSE, INFINITE);
    cb.push_imm(0);
    cb.push_imm(0);
    cb.push_imm(reinterpret_cast<int>(rexited));
    cb.mov_imm(EAX, reinterpret_cast<int>(
        resolver_.Resolve("ntdll!NtWaitForSingleObject")));
    cb.call(EAX);

    ResumeOriginalFunction(kFuncName, moved_instructions, kStubOffset, &cb);
  }


  void Patch() {
    if (options_.vista()) {
      // TODO(deanm): Make PatchCreateThread work on Vista.
      PatchThreadBeginVista();
    } else {
      PatchCreateThread();
      PatchThreadBegin();
    }

    PatchThreadExit();
    PatchSetThreadName();
    PatchSyscall();

    PatchApcDispatcher();

    // The loader logging needs to be improved a bit to really be useful.
    //PatchLoader();

    // These are interesting, but will collect a ton of data:
    if (options_.log_heap()) {
      PatchAllocateHeap();
      PatchFreeHeap();
    }
    if (options_.log_lock()) {
      PatchEnterCriticalSection();
      PatchTryEnterCriticalSection();
      PatchLeaveCriticalSection();
    }
  }

  // Dump the event records from the playground to stdout in a JSON format.
  // TODO: Drop RDTSCNormalizer, it was from old code that tried to use the
  // rdtsc counters from the CPU, and this required a bunch of normalization
  // to account for non-syncronized timestamps across different cores, etc.
  void DumpJSON(RDTSCNormalizer* rdn, SymResolver* res) {
    int pos = kOffLogAreaPtr;
    int i = IntAt(pos);
    pos += 4;

    std::map<int, const char*> syscalls = CreateSyscallMap();

    printf("parseEvents([\n");
    for (int end = pos + i; pos < end; ) {
      printf("{\n");
      __int64 ts = Int64At(pos);
      pos += 8;
      void* cpuid = reinterpret_cast<void*>(IntAt(pos));
      pos += 4;
      printf("'ms': %f,\n", rdn->MsFromStart(cpuid, ts));

      printf("'cpu': 0x%x,\n'thread': 0x%x,\n", cpuid, IntAt(pos));
      pos += 4;

      if (options_.stack_unwind_depth() > 0) {
        printf("'stacktrace': [\n");
        for (int i = 0; i < options_.stack_unwind_depth(); ++i) {
          int retaddr = IntAt(pos + (i * 4));
          if (!retaddr)
            break;
          printf("  [ 0x%x, %s ],\n",
                 retaddr,
                 res ? JSONString(res->Unresolve(retaddr)).c_str() : "\"\"");
        }
        printf("],\n");
        pos += (options_.stack_unwind_depth() * 4);
      }


      EventRecordType rt = static_cast<EventRecordType>(IntAt(pos));
      pos += 4;

      switch (rt) {
        case EVENT_TYPE_LDR:
        {
          printf("'eventtype': 'EVENT_TYPE_LDR',\n");
          std::string str(&buf_[pos], kLdrBufSize);
          str = str.substr(0, str.find('\0'));
          printf("'ldrinfo': %s,\n", JSONString(str).c_str());
          pos += kLdrBufSize;
          break;
        }
        case EVENT_TYPE_CREATETHREAD:
        {
          printf("'eventtype': 'EVENT_TYPE_CREATETHREAD',\n"
                 "'eventid': 0x%x,\n"
                 "'startaddr': 0x%x,\n",
                 IntAt(pos), IntAt(pos+4));
          pos += 8;
          break;
        }
        case EVENT_TYPE_THREADBEGIN:
        {
          printf("'eventtype': 'EVENT_TYPE_THREADBEGIN',\n"
                 "'parenteventid': 0x%x,\n"
                 "'startaddr': 0x%x,\n",
                 IntAt(pos), IntAt(pos+4));
          pos += 8;
          break;
        }
        case EVENT_TYPE_THREADNAME:
        {
          std::string str(&buf_[pos], kThreadNameBufSize);
          str = str.substr(0, str.find('\0'));
          printf("'eventtype': 'EVENT_TYPE_THREADNAME',\n"
                 "'threadname': %s,\n",
                 JSONString(str).c_str());
          pos += kThreadNameBufSize;
          break;
        }
        case EVENT_TYPE_PROCESSEXIT:
        {
          printf("'eventtype': 'EVENT_TYPE_PROCESSEXIT',\n");
          break;
        }
        case EVENT_TYPE_THREADEXIT:
        {
          printf("'eventtype': 'EVENT_TYPE_THREADEXIT',\n");
          break;
        }
        case EVENT_TYPE_ALLOCHEAP:
        {
          printf("'eventtype': 'EVENT_TYPE_ALLOCHEAP',\n"
                 "'heaphandle': 0x%x,\n"
                 "'heapflags': 0x%x,\n"
                 "'heapsize': %d,\n",
                 IntAt(pos), IntAt(pos+4), IntAt(pos+8));
          pos += 12;
          break;
        }
        case EVENT_TYPE_FREEHEAP:
        {
          printf("'eventtype': 'EVENT_TYPE_FREEHEAP',\n"
                 "'heaphandle': 0x%x,\n"
                 "'heapflags': 0x%x,\n"
                 "'heapptr': %d,\n",
                 IntAt(pos), IntAt(pos+4), IntAt(pos+8));
          pos += 12;
          break;
        }
        case EVENT_TYPE_SYSCALL:
        {
          int syscall = IntAt(pos);
          printf("'eventtype': 'EVENT_TYPE_SYSCALL',\n"
                 "'syscall': 0x%x,\n", syscall);
          pos += 16;

          printf("'syscallargs': [\n");
          for (int i = 0; i < 3; ++i) {
            printf("  0x%x,\n", IntAt(pos));
            pos += 4;
          }
          printf("],\n");

          printf("'retval': 0x%x,\n"
                 "'done': %f,\n",
                 IntAt(pos), rdn->MsFromStart(0, Int64At(pos+4)));
          pos += 12;

          if (syscalls.count(syscall) == 1) {
            std::string sname = syscalls[syscall];
            printf("'syscallname': %s,\n",
                   JSONString(sname).c_str());
            // Mark system calls that we should consider "waiting" system
            // calls, where we are not actually active.
            if (sname.find("WaitFor") != std::string::npos ||
                sname.find("RemoveIoCompletion") != std::string::npos) {
              printf("'waiting': 1,\n");
            }
          }
          break;
        }
        case EVENT_TYPE_ENTER_CS:
        {
          printf("'eventtype': 'EVENT_TYPE_ENTER_CS',\n"
                 "'critical_section': 0x%x,\n", IntAt(pos));
          pos += 4;
          break;
        }
        case EVENT_TYPE_TRYENTER_CS:
        {
          printf("'eventtype': 'EVENT_TYPE_TRYENTER_CS',\n"
                 "'critical_section': 0x%x,\n"
                 "'retval': 0x%x,\n",
                 IntAt(pos), IntAt(pos+4));
          pos += 8;
          break;
        }
        case EVENT_TYPE_LEAVE_CS:
        {
          printf("'eventtype': 'EVENT_TYPE_LEAVE_CS',\n"
                 "'critical_section': 0x%x,\n", IntAt(pos));
          pos += 4;
          break;
        }
        case EVENT_TYPE_APC:
        {
          int func_addr = IntAt(pos);
          printf("'eventtype': 'EVENT_TYPE_APC',\n"
                 "'func_addr': 0x%x,\n"
                 "'func_addr_name': %s,\n"
                 "'ret_addr': 0x%x,\n"
                 "'done': %f,\n",
                 func_addr,
                 res ? JSONString(res->Unresolve(func_addr)).c_str() : "\"\"",
                 IntAt(pos+4), rdn->MsFromStart(0, Int64At(pos+8)));
          pos += 16;
          break;
        }
        default:
          NOTREACHED("Unknown event type: %d", rt);
          break;
      }
      printf("},\n");
    }
    printf("]);");
  }

  int IntAt(int pos) { return *reinterpret_cast<int*>(&buf_[pos]); }
  __int64 Int64At(int pos) { return *reinterpret_cast<__int64*>(&buf_[pos]); }


 private:
  // Handle the process we install into or read back from.
  HANDLE proc_;
  // The address where we will keep our playground in the remote process.
  char* remote_addr_;
  // Lookup addresses from symbol names for ntdll.dll.
  SymResolver resolver_;
  Options options_;
  // A local copy of the playground data, we copy it into the remote process.
  char buf_[kPlaygroundSize];
};


int main(int argc, char** argv) {
  std::string command_line;
  bool use_symbols = false;
  bool attaching = false;
  bool launched = false;
  bool manual_quit = false;

  Playground::Options options;

  PROCESS_INFORMATION info = {0};

  argc--; argv++;

  while (argc > 0) {
    if (std::string("--symbols") == argv[0]) {
      use_symbols = true;
    } else if (std::string("--vista") == argv[0]) {
      options.set_vista(true);
    } else if (std::string("--log-heap") == argv[0]) {
      options.set_log_heap(true);
    } else if (std::string("--log-lock") == argv[0]) {
      options.set_log_lock(true);
    } else if (std::string("--manual-quit") == argv[0]) {
      manual_quit = true;
    } else if (argc >= 2 && std::string("--unwind") == argv[0]) {
      options.set_stack_unwind_depth(atoi(argv[1]));
      argc--; argv++;
    } else if (argc >= 2 && !launched && std::string("--attach") == argv[0]) {
      attaching = true;
      info.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, atoi(argv[1]));
      launched = true;
      argc--; argv++;
    } else if (!launched) {
      STARTUPINFOA start_info = {0};
      start_info.cb = sizeof(start_info);

      if (!CreateProcessA(NULL,
                          argv[0],
                          NULL,
                          NULL,
                          FALSE,
                          CREATE_SUSPENDED,
                          NULL,
                          NULL,
                          &start_info,
                          &info)) {
        NOTREACHED("Failed to launch \"%s\": %d\n", argv[0], GetLastError());
        return 1;
      }
      launched = true;
    } else {
      NOTREACHED("error parsing command line.");
    }
    argc--; argv++;
  }

  if (!launched) {
    printf("usage: traceline.exe \"app.exe my arguments\"\n"
           "  --attach 123: buggy support for attaching to a process\n"
           "  --unwind 16: unwind the stack to the specified max depth\n"
           "  --symbols: use symbols for stacktraces\n"
           "  --log-heap: log heap operations (alloc / free).\n"
           "  --log-lock: log lock (critical section) operations.\n");
    return 1;
  }


  HANDLE exiting = CreateEvent(NULL, FALSE, FALSE, NULL);
  HANDLE exited = CreateEvent(NULL, FALSE, FALSE, NULL);

  // The playground object is big (32MB), dynamically alloc.
  Playground* pg = new Playground(info.hProcess, options);

  pg->AllocateInRemote();
  pg->Patch();
  pg->PatchExit(exiting, exited);
  pg->CopyToRemote();

  RDTSCNormalizer rdn;
  rdn.Start();

  if (!attaching)
    ResumeThread(info.hThread);

  // Wait until we have been notified that it's exiting.
  if (manual_quit) {
    fprintf(stderr, "Press enter when you want stop tracing and collect.\n");
    fflush(stderr);
    getchar();
  } else {
    HANDLE whs[] = {exiting, info.hProcess};
    if (WaitForMultipleObjects(2, whs, FALSE, INFINITE) != WAIT_OBJECT_0) {
      NOTREACHED("Failed to correctly capture process shutdown.");
    }
  }

  pg->CopyFromRemote();

  if (use_symbols) {
    // Break in and get the symbols...
    SymResolver res(NULL, info.hProcess);
    pg->DumpJSON(&rdn, &res);
  } else {
    pg->DumpJSON(&rdn, NULL);
  }

  // Notify that it can exit now, we are done.
  SetEvent(exited);

  CloseHandle(info.hProcess);
  CloseHandle(info.hThread);

  delete pg;
}
