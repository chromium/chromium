// Copyright 2018 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/sanitized/process_snapshot_sanitized.h"

#include <string.h>

#include "base/macros.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "util/file/file_io.h"
#include "util/misc/address_sanitizer.h"
#include "util/numeric/safe_assignment.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include <sys/syscall.h>

#include "snapshot/linux/process_snapshot_linux.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/linux/exception_information.h"
#include "util/posix/signals.h"
#endif

namespace crashpad {
namespace test {
namespace {

class ExceptionGenerator {
 public:
  static ExceptionGenerator* Get() {
    static ExceptionGenerator* instance = new ExceptionGenerator();
    return instance;
  }

  bool Initialize(FileHandle in, FileHandle out) {
    in_ = in;
    out_ = out;
    return Signals::InstallCrashHandlers(HandleCrash, 0, nullptr);
  }

 private:
  ExceptionGenerator() = default;
  ~ExceptionGenerator() = delete;

  static void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    auto state = Get();

    ExceptionInformation info = {};
    info.siginfo_address = FromPointerCast<VMAddress>(siginfo);
    info.context_address = FromPointerCast<VMAddress>(context);
    info.thread_id = syscall(SYS_gettid);

    auto info_addr = FromPointerCast<VMAddress>(&info);
    ASSERT_TRUE(LoggingWriteFile(state->out_, &info_addr, sizeof(info_addr)));

    CheckedReadFileAtEOF(state->in_);
    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, nullptr);
  }

  FileHandle in_;
  FileHandle out_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionGenerator);
};

constexpr char kAllowedAnnotationName[] = "name_of_allowed_anno";
constexpr char kAllowedAnnotationValue[] = "some_value";
constexpr char kNonAllowedAnnotationName[] = "non_allowed_anno";
constexpr char kNonAllowedAnnotationValue[] = "private_annotation";
constexpr char kSensitiveStackData[] = "sensitive_stack_data";

struct ChildTestAddresses {
  VMAddress string_address;
  VMAddress module_address;
  VMAddress non_module_address;
  VMAddress code_pointer_address;
  VMAddress code_pointer_value;
};

void ChildTestFunction() {
  FileHandle in = StdioFileHandle(StdioStream::kStandardInput);
  FileHandle out = StdioFileHandle(StdioStream::kStandardOutput);

  static StringAnnotation<32> allowed_annotation(kAllowedAnnotationName);
  allowed_annotation.Set(kAllowedAnnotationValue);

  static StringAnnotation<32> non_allowed_annotation(kNonAllowedAnnotationName);
  non_allowed_annotation.Set(kNonAllowedAnnotationValue);

  char string_data[base::size(kSensitiveStackData)];
  strcpy(string_data, kSensitiveStackData);

  void (*code_pointer)(void) = ChildTestFunction;

  ChildTestAddresses addrs = {};
  addrs.string_address = FromPointerCast<VMAddress>(string_data);
  addrs.module_address = FromPointerCast<VMAddress>(ChildTestFunction);
  addrs.non_module_address = FromPointerCast<VMAddress>(&addrs);
  addrs.code_pointer_address = FromPointerCast<VMAddress>(&code_pointer);
  addrs.code_pointer_value = FromPointerCast<VMAddress>(code_pointer);
  ASSERT_TRUE(LoggingWriteFile(out, &addrs, sizeof(addrs)));

  auto gen = ExceptionGenerator::Get();
  ASSERT_TRUE(gen->Initialize(in, out));

  __builtin_trap();
}

CRASHPAD_CHILD_TEST_MAIN(ChildToBeSanitized) {
  ChildTestFunction();
  NOTREACHED();
  return EXIT_SUCCESS;
}

void ExpectAnnotations(ProcessSnapshot* snapshot, bool sanitized) {
  bool found_allowed = false;
  bool found_non_allowed = false;
  for (auto module : snapshot->Modules()) {
    for (const auto& anno : module->AnnotationObjects()) {
      if (anno.name == kAllowedAnnotationName) {
        found_allowed = true;
      } else if (anno.name == kNonAllowedAnnotationName) {
        found_non_allowed = true;
      }
    }
  }

  EXPECT_TRUE(found_allowed);
  if (sanitized) {
    EXPECT_FALSE(found_non_allowed);
  } else {
    EXPECT_TRUE(found_non_allowed);
  }
}

void ExpectProcessMemory(ProcessSnapshot* snapshot,
                         VMAddress allowed_byte,
                         bool sanitized) {
  auto memory = snapshot->Memory();

  char out;
  EXPECT_TRUE(memory->Read(allowed_byte, 1, &out));

  bool disallowed_read = memory->Read(allowed_byte + 1, 1, &out);
  if (sanitized) {
    EXPECT_FALSE(disallowed_read);
  } else {
    EXPECT_TRUE(disallowed_read);
  }
}

class StackSanitizationChecker : public MemorySnapshot::Delegate {
 public:
  StackSanitizationChecker() = default;
  ~StackSanitizationChecker() = default;

  void CheckStack(const MemorySnapshot* stack,
                  const ChildTestAddresses& addrs,
                  bool is_64_bit,
                  bool sanitized) {
    stack_ = stack;
    addrs_ = addrs;
    is_64_bit_ = is_64_bit;
    sanitized_ = sanitized;
    EXPECT_TRUE(stack_->Read(this));
  }

  // MemorySnapshot::Delegate
  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    // AddressSanitizer with use-after-return detection causes stack variables
    // to be allocated on the heap.
#if !defined(ADDRESS_SANITIZER)
    size_t pointer_offset;
    if (!AssignIfInRange(&pointer_offset,
                         addrs_.code_pointer_address - stack_->Address())) {
      ADD_FAILURE();
      return false;
    }

    const auto data_c = static_cast<char*>(data);
    VMAddress pointer_value;
    if (is_64_bit_) {
      pointer_value = *reinterpret_cast<uint64_t*>(data_c + pointer_offset);
    } else {
      pointer_value = *reinterpret_cast<uint32_t*>(data_c + pointer_offset);
    }
    EXPECT_EQ(pointer_value, addrs_.code_pointer_value);

    size_t string_offset;
    if (!AssignIfInRange(&string_offset,
                         addrs_.string_address - stack_->Address())) {
      ADD_FAILURE();
      return false;
    }

    auto string = data_c + string_offset;
    if (sanitized_) {
      EXPECT_STRNE(string, kSensitiveStackData);
    } else {
      EXPECT_STREQ(string, kSensitiveStackData);
    }
#endif  // !ADDRESS_SANITIZER
    return true;
  }

 private:
  const MemorySnapshot* stack_;
  ChildTestAddresses addrs_;
  bool is_64_bit_;
  bool sanitized_;
};

void ExpectStackData(ProcessSnapshot* snapshot,
                     const ChildTestAddresses& addrs,
                     bool sanitized) {
  const ThreadSnapshot* crasher = nullptr;
  for (const auto thread : snapshot->Threads()) {
    if (thread->ThreadID() == snapshot->Exception()->ThreadID()) {
      crasher = thread;
      break;
    }
  }
  ASSERT_TRUE(crasher);

  const MemorySnapshot* stack = crasher->Stack();
  StackSanitizationChecker().CheckStack(
      stack, addrs, crasher->Context()->Is64Bit(), sanitized);
}

class SanitizeTest : public MultiprocessExec {
 public:
  SanitizeTest() : MultiprocessExec() {
    SetChildTestMainFunction("ChildToBeSanitized");
    SetExpectedChildTerminationBuiltinTrap();
  }

  ~SanitizeTest() = default;

 private:
  void MultiprocessParent() {
    ChildTestAddresses addrs = {};
    ASSERT_TRUE(
        LoggingReadFileExactly(ReadPipeHandle(), &addrs, sizeof(addrs)));

    VMAddress exception_info_addr;
    ASSERT_TRUE(LoggingReadFileExactly(
        ReadPipeHandle(), &exception_info_addr, sizeof(exception_info_addr)));

    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(ChildProcess()));

    ProcessSnapshotLinux snapshot;
    ASSERT_TRUE(snapshot.Initialize(&connection));
    ASSERT_TRUE(snapshot.InitializeException(exception_info_addr));

    ExpectAnnotations(&snapshot, /* sanitized= */ false);
    ExpectStackData(&snapshot, addrs, /* sanitized= */ false);
    ExpectProcessMemory(&snapshot,
                        addrs.string_address,
                        /* sanitized= */ false);

    auto allowed_annotations = std::make_unique<std::vector<std::string>>();
    allowed_annotations->push_back(kAllowedAnnotationName);

    auto allowed_memory_ranges =
        std::make_unique<std::vector<std::pair<VMAddress, VMAddress>>>();
    allowed_memory_ranges->push_back(
        std::make_pair(addrs.string_address, addrs.string_address + 1));

    ProcessSnapshotSanitized sanitized;
    ASSERT_TRUE(sanitized.Initialize(&snapshot,
                                     std::move(allowed_annotations),
                                     std::move(allowed_memory_ranges),
                                     addrs.module_address,
                                     true));

    ExpectAnnotations(&sanitized, /* sanitized= */ true);
    ExpectStackData(&sanitized, addrs, /* sanitized= */ true);
    ExpectProcessMemory(&sanitized,
                        addrs.string_address,
                        /* sanitized= */ true);

    ProcessSnapshotSanitized screened_snapshot;
    EXPECT_FALSE(screened_snapshot.Initialize(
        &snapshot, nullptr, nullptr, addrs.non_module_address, false));
  }

  DISALLOW_COPY_AND_ASSIGN(SanitizeTest);
};

TEST(ProcessSnapshotSanitized, Sanitize) {
  SanitizeTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
