// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "snapshot/linux/process_reader_linux.h"

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/memory/free_deleter.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "snapshot/linux/test_modules.h"
#include "test/errors.h"
#include "test/linux/fake_ptrace_connection.h"
#include "test/linux/get_tls.h"
#include "test/multiprocess.h"
#include "test/scoped_module_handle.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/file/file_writer.h"
#include "util/file/filesystem.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/misc/address_sanitizer.h"
#include "util/misc/from_pointer_cast.h"
#include "util/misc/memory_sanitizer.h"
#include "util/synchronization/semaphore.h"

#if defined(OS_ANDROID)
#include <android/api-level.h>
#include <android/set_abort_message.h>
#include "dlfcn_internal.h"

// Normally this comes from set_abort_message.h, but only at API level 21.
extern "C" void android_set_abort_message(const char* msg)
    __attribute__((weak));
#endif

namespace crashpad {
namespace test {
namespace {

pid_t gettid() {
  return syscall(SYS_gettid);
}

TEST(ProcessReaderLinux, SelfBasic) {
  FakePtraceConnection connection;
  connection.Initialize(getpid());

  ProcessReaderLinux process_reader;
  ASSERT_TRUE(process_reader.Initialize(&connection));

#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(process_reader.Is64Bit());
#else
  EXPECT_FALSE(process_reader.Is64Bit());
#endif

  EXPECT_EQ(process_reader.ProcessID(), getpid());
  EXPECT_EQ(process_reader.ParentProcessID(), getppid());

  static constexpr char kTestMemory[] = "Some test memory";
  char buffer[base::size(kTestMemory)];
  ASSERT_TRUE(process_reader.Memory()->Read(
      reinterpret_cast<LinuxVMAddress>(kTestMemory),
      sizeof(kTestMemory),
      &buffer));
  EXPECT_STREQ(kTestMemory, buffer);

  EXPECT_EQ("", process_reader.AbortMessage());
}

constexpr char kTestMemory[] = "Read me from another process";

class BasicChildTest : public Multiprocess {
 public:
  BasicChildTest() : Multiprocess() {}
  ~BasicChildTest() {}

 private:
  void MultiprocessParent() override {
    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(ChildPID()));

    ProcessReaderLinux process_reader;
    ASSERT_TRUE(process_reader.Initialize(&connection));

#if !defined(ARCH_CPU_64_BITS)
    EXPECT_FALSE(process_reader.Is64Bit());
#else
    EXPECT_TRUE(process_reader.Is64Bit());
#endif

    EXPECT_EQ(process_reader.ParentProcessID(), getpid());
    EXPECT_EQ(process_reader.ProcessID(), ChildPID());

    std::string read_string;
    ASSERT_TRUE(process_reader.Memory()->ReadCString(
        reinterpret_cast<LinuxVMAddress>(kTestMemory), &read_string));
    EXPECT_EQ(read_string, kTestMemory);
  }

  void MultiprocessChild() override { CheckedReadFileAtEOF(ReadPipeHandle()); }

  DISALLOW_COPY_AND_ASSIGN(BasicChildTest);
};

TEST(ProcessReaderLinux, ChildBasic) {
  BasicChildTest test;
  test.Run();
}

class TestThreadPool {
 public:
  struct ThreadExpectation {
    LinuxVMAddress tls = 0;
    LinuxVMAddress stack_address = 0;
    LinuxVMSize max_stack_size = 0;
    int sched_policy = 0;
    int static_priority = 0;
    int nice_value = 0;
  };

  TestThreadPool() : threads_() {}

  ~TestThreadPool() {
    for (const auto& thread : threads_) {
      thread->exit_semaphore.Signal();
    }

    for (const auto& thread : threads_) {
      EXPECT_EQ(pthread_join(thread->pthread, nullptr), 0)
          << ErrnoMessage("pthread_join");
    }
  }

  void StartThreads(size_t thread_count, size_t stack_size = 0) {
    for (size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
      threads_.push_back(std::make_unique<Thread>());
      Thread* thread = threads_.back().get();

      pthread_attr_t attr;
      ASSERT_EQ(pthread_attr_init(&attr), 0)
          << ErrnoMessage("pthread_attr_init");

      if (stack_size > 0) {
        void* stack_ptr;
        errno = posix_memalign(&stack_ptr, getpagesize(), stack_size);
        ASSERT_EQ(errno, 0) << ErrnoMessage("posix_memalign");

        thread->stack.reset(reinterpret_cast<char*>(stack_ptr));

        ASSERT_EQ(pthread_attr_setstack(&attr, thread->stack.get(), stack_size),
                  0)
            << ErrnoMessage("pthread_attr_setstack");
        thread->expectation.max_stack_size = stack_size;
      }

      ASSERT_EQ(pthread_attr_setschedpolicy(&attr, SCHED_OTHER), 0)
          << ErrnoMessage("pthread_attr_setschedpolicy");
      thread->expectation.sched_policy = SCHED_OTHER;

      sched_param param;
      param.sched_priority = 0;
      ASSERT_EQ(pthread_attr_setschedparam(&attr, &param), 0)
          << ErrnoMessage("pthread_attr_setschedparam");
      thread->expectation.static_priority = 0;

      thread->expectation.nice_value = thread_index % 20;

      ASSERT_EQ(pthread_create(&thread->pthread, &attr, ThreadMain, thread), 0)
          << ErrnoMessage("pthread_create");
    }

    for (const auto& thread : threads_) {
      thread->ready_semaphore.Wait();
    }
  }

  pid_t GetThreadExpectation(size_t thread_index,
                             ThreadExpectation* expectation) {
    CHECK_LT(thread_index, threads_.size());

    const Thread* thread = threads_[thread_index].get();
    *expectation = thread->expectation;
    return thread->tid;
  }

 private:
  struct Thread {
    Thread()
        : pthread(),
          expectation(),
          ready_semaphore(0),
          exit_semaphore(0),
          tid(-1) {}
    ~Thread() {}

    pthread_t pthread;
    ThreadExpectation expectation;
    std::unique_ptr<char[], base::FreeDeleter> stack;
    Semaphore ready_semaphore;
    Semaphore exit_semaphore;
    pid_t tid;
  };

  static void* ThreadMain(void* argument) {
    Thread* thread = static_cast<Thread*>(argument);

    CHECK_EQ(setpriority(PRIO_PROCESS, 0, thread->expectation.nice_value), 0)
        << ErrnoMessage("setpriority");

    thread->expectation.tls = GetTLS();
    thread->expectation.stack_address =
        reinterpret_cast<LinuxVMAddress>(&thread);
    thread->tid = gettid();

    thread->ready_semaphore.Signal();
    thread->exit_semaphore.Wait();

    CHECK_EQ(pthread_self(), thread->pthread);

    return nullptr;
  }

  std::vector<std::unique_ptr<Thread>> threads_;

  DISALLOW_COPY_AND_ASSIGN(TestThreadPool);
};

using ThreadMap = std::map<pid_t, TestThreadPool::ThreadExpectation>;

void ExpectThreads(const ThreadMap& thread_map,
                   const std::vector<ProcessReaderLinux::Thread>& threads,
                   PtraceConnection* connection) {
  ASSERT_EQ(threads.size(), thread_map.size());

  MemoryMap memory_map;
  ASSERT_TRUE(memory_map.Initialize(connection));

  for (const auto& thread : threads) {
    SCOPED_TRACE(
        base::StringPrintf("Thread id %d, tls 0x%" PRIx64
                           ", stack addr 0x%" PRIx64 ", stack size 0x%" PRIx64,
                           thread.tid,
                           thread.thread_info.thread_specific_data_address,
                           thread.stack_region_address,
                           thread.stack_region_size));

    const auto& iterator = thread_map.find(thread.tid);
    ASSERT_NE(iterator, thread_map.end());

    EXPECT_EQ(thread.thread_info.thread_specific_data_address,
              iterator->second.tls);

    ASSERT_TRUE(memory_map.FindMapping(thread.stack_region_address));
    ASSERT_TRUE(memory_map.FindMapping(thread.stack_region_address +
                                       thread.stack_region_size - 1));

#if !defined(ADDRESS_SANITIZER)
    // AddressSanitizer causes stack variables to be stored separately from the
    // call stack.
    EXPECT_LE(thread.stack_region_address, iterator->second.stack_address);
    EXPECT_GE(thread.stack_region_address + thread.stack_region_size,
              iterator->second.stack_address);
#endif  // !defined(ADDRESS_SANITIZER)

    if (iterator->second.max_stack_size) {
      EXPECT_LT(thread.stack_region_size, iterator->second.max_stack_size);
    }

    EXPECT_EQ(thread.sched_policy, iterator->second.sched_policy);
    EXPECT_EQ(thread.static_priority, iterator->second.static_priority);
    EXPECT_EQ(thread.nice_value, iterator->second.nice_value);
  }
}

class ChildThreadTest : public Multiprocess {
 public:
  ChildThreadTest(size_t stack_size = 0)
      : Multiprocess(), stack_size_(stack_size) {}
  ~ChildThreadTest() {}

 private:
  void MultiprocessParent() override {
    ThreadMap thread_map;
    for (size_t thread_index = 0; thread_index < kThreadCount + 1;
         ++thread_index) {
      pid_t tid;
      TestThreadPool::ThreadExpectation expectation;

      CheckedReadFileExactly(ReadPipeHandle(), &tid, sizeof(tid));
      CheckedReadFileExactly(
          ReadPipeHandle(), &expectation, sizeof(expectation));
      thread_map[tid] = expectation;
    }

    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(ChildPID()));

    ProcessReaderLinux process_reader;
    ASSERT_TRUE(process_reader.Initialize(&connection));
    const std::vector<ProcessReaderLinux::Thread>& threads =
        process_reader.Threads();
    ExpectThreads(thread_map, threads, &connection);
  }

  void MultiprocessChild() override {
    TestThreadPool thread_pool;
    thread_pool.StartThreads(kThreadCount, stack_size_);

    TestThreadPool::ThreadExpectation expectation;
#if defined(MEMORY_SANITIZER)
    // memset() + re-initialization is required to zero padding bytes for MSan.
    memset(&expectation, 0, sizeof(expectation));
#endif  // defined(MEMORY_SANITIZER)
    expectation = {};
    expectation.tls = GetTLS();
    expectation.stack_address = reinterpret_cast<LinuxVMAddress>(&thread_pool);

    int res = sched_getscheduler(0);
    ASSERT_GE(res, 0) << ErrnoMessage("sched_getscheduler");
    expectation.sched_policy = res;

    sched_param param;
    ASSERT_EQ(sched_getparam(0, &param), 0) << ErrnoMessage("sched_getparam");
    expectation.static_priority = param.sched_priority;

    errno = 0;
    res = getpriority(PRIO_PROCESS, 0);
    ASSERT_FALSE(res == -1 && errno) << ErrnoMessage("getpriority");
    expectation.nice_value = res;

    pid_t tid = gettid();

    CheckedWriteFile(WritePipeHandle(), &tid, sizeof(tid));
    CheckedWriteFile(WritePipeHandle(), &expectation, sizeof(expectation));

    for (size_t thread_index = 0; thread_index < kThreadCount; ++thread_index) {
      tid = thread_pool.GetThreadExpectation(thread_index, &expectation);
      CheckedWriteFile(WritePipeHandle(), &tid, sizeof(tid));
      CheckedWriteFile(WritePipeHandle(), &expectation, sizeof(expectation));
    }

    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  static constexpr size_t kThreadCount = 3;
  const size_t stack_size_;

  DISALLOW_COPY_AND_ASSIGN(ChildThreadTest);
};

TEST(ProcessReaderLinux, ChildWithThreads) {
  ChildThreadTest test;
  test.Run();
}

TEST(ProcessReaderLinux, ChildThreadsWithSmallUserStacks) {
  ChildThreadTest test(PTHREAD_STACK_MIN);
  test.Run();
}

// Tests a thread with a stack that spans multiple mappings.
class ChildWithSplitStackTest : public Multiprocess {
 public:
  ChildWithSplitStackTest() : Multiprocess(), page_size_(getpagesize()) {}
  ~ChildWithSplitStackTest() {}

 private:
  void MultiprocessParent() override {
    LinuxVMAddress stack_addr1;
    LinuxVMAddress stack_addr2;
    LinuxVMAddress stack_addr3;

    CheckedReadFileExactly(ReadPipeHandle(), &stack_addr1, sizeof(stack_addr1));
    CheckedReadFileExactly(ReadPipeHandle(), &stack_addr2, sizeof(stack_addr2));
    CheckedReadFileExactly(ReadPipeHandle(), &stack_addr3, sizeof(stack_addr3));

    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(ChildPID()));

    ProcessReaderLinux process_reader;
    ASSERT_TRUE(process_reader.Initialize(&connection));

    const std::vector<ProcessReaderLinux::Thread>& threads =
        process_reader.Threads();
    ASSERT_EQ(threads.size(), 1u);

    LinuxVMAddress thread_stack_start = threads[0].stack_region_address;
    EXPECT_LE(thread_stack_start, stack_addr1);
    EXPECT_LE(thread_stack_start, stack_addr2);
    EXPECT_LE(thread_stack_start, stack_addr3);

    LinuxVMAddress thread_stack_end =
        thread_stack_start + threads[0].stack_region_size;
    EXPECT_GE(thread_stack_end, stack_addr1);
    EXPECT_GE(thread_stack_end, stack_addr2);
    EXPECT_GE(thread_stack_end, stack_addr3);
  }

  void MultiprocessChild() override {
    const LinuxVMSize stack_size = page_size_ * 4;
    GrowStack(stack_size, reinterpret_cast<LinuxVMAddress>(&stack_size));
  }

  void GrowStack(LinuxVMSize stack_size, LinuxVMAddress bottom_of_stack) {
    char stack_contents[4096];
    auto stack_address = reinterpret_cast<LinuxVMAddress>(&stack_contents);

    if (bottom_of_stack - stack_address < stack_size) {
      GrowStack(stack_size, bottom_of_stack);
    } else {
      // Write-protect a page on our stack to split up the mapping
      LinuxVMAddress page_addr =
          stack_address - (stack_address % page_size_) + 2 * page_size_;
      ASSERT_EQ(
          mprotect(reinterpret_cast<void*>(page_addr), page_size_, PROT_READ),
          0)
          << ErrnoMessage("mprotect");

      CheckedWriteFile(
          WritePipeHandle(), &bottom_of_stack, sizeof(bottom_of_stack));
      CheckedWriteFile(WritePipeHandle(), &page_addr, sizeof(page_addr));
      CheckedWriteFile(
          WritePipeHandle(), &stack_address, sizeof(stack_address));

      // Wait for parent to read us
      CheckedReadFileAtEOF(ReadPipeHandle());

      ASSERT_EQ(mprotect(reinterpret_cast<void*>(page_addr),
                         page_size_,
                         PROT_READ | PROT_WRITE),
                0)
          << ErrnoMessage("mprotect");
    }
  }

  const size_t page_size_;

  DISALLOW_COPY_AND_ASSIGN(ChildWithSplitStackTest);
};

// AddressSanitizer with use-after-return detection causes stack variables to
// be allocated on the heap.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ChildWithSplitStack DISABLED_ChildWithSplitStack
#else
#define MAYBE_ChildWithSplitStack ChildWithSplitStack
#endif
TEST(ProcessReaderLinux, MAYBE_ChildWithSplitStack) {
  ChildWithSplitStackTest test;
  test.Run();
}

// Android doesn't provide dl_iterate_phdr on ARM until API 21.
#if !defined(OS_ANDROID) || !defined(ARCH_CPU_ARMEL) || __ANDROID_API__ >= 21
int ExpectFindModule(dl_phdr_info* info, size_t size, void* data) {
  SCOPED_TRACE(
      base::StringPrintf("module %s at 0x%" PRIx64 " phdrs 0x%" PRIx64,
                         info->dlpi_name,
                         LinuxVMAddress{info->dlpi_addr},
                         FromPointerCast<LinuxVMAddress>(info->dlpi_phdr)));
  auto modules =
      reinterpret_cast<const std::vector<ProcessReaderLinux::Module>*>(data);


#if defined(OS_ANDROID)
  // Prior to API 27, Bionic includes a null entry for /system/bin/linker.
  if (!info->dlpi_name) {
    EXPECT_EQ(info->dlpi_addr, 0u);
    EXPECT_EQ(info->dlpi_phnum, 0u);
    EXPECT_EQ(info->dlpi_phdr, nullptr);
    return 0;
  }
#endif

  // Bionic doesn't always set both of these addresses for the vdso and
  // /system/bin/linker, but it does always set one of them.
  VMAddress module_addr = info->dlpi_phdr
                              ? FromPointerCast<LinuxVMAddress>(info->dlpi_phdr)
                              : info->dlpi_addr;

  // TODO(jperaza): This can use a range map when one is available.
  bool found = false;
  for (const auto& module : *modules) {
    if (module.elf_reader && module_addr >= module.elf_reader->Address() &&
        module_addr <
            module.elf_reader->Address() + module.elf_reader->Size()) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
  return 0;
}
#endif  // !OS_ANDROID || !ARCH_CPU_ARMEL || __ANDROID_API__ >= 21

void ExpectModulesFromSelf(
    const std::vector<ProcessReaderLinux::Module>& modules) {
  for (const auto& module : modules) {
    EXPECT_FALSE(module.name.empty());
    EXPECT_NE(module.type, ModuleSnapshot::kModuleTypeUnknown);
  }

// Android doesn't provide dl_iterate_phdr on ARM until API 21.
#if !defined(OS_ANDROID) || !defined(ARCH_CPU_ARMEL) || __ANDROID_API__ >= 21
  EXPECT_EQ(
      dl_iterate_phdr(
          ExpectFindModule,
          reinterpret_cast<void*>(
              const_cast<std::vector<ProcessReaderLinux::Module>*>(&modules))),
      0);
#endif  // !OS_ANDROID || !ARCH_CPU_ARMEL || __ANDROID_API__ >= 21
}

#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
void ExpectTestModule(ProcessReaderLinux* reader,
                      const std::string& module_name) {
  for (const auto& module : reader->Modules()) {
    if (module.name.find(module_name) != std::string::npos) {
      ASSERT_TRUE(module.elf_reader);

      VMAddress dynamic_addr;
      ASSERT_TRUE(module.elf_reader->GetDynamicArrayAddress(&dynamic_addr));

      auto dynamic_mapping = reader->GetMemoryMap()->FindMapping(dynamic_addr);
      auto mappings =
          reader->GetMemoryMap()->FindFilePossibleMmapStarts(*dynamic_mapping);
      EXPECT_EQ(mappings->Count(), 2u);
      return;
    }
  }
  ADD_FAILURE() << "Test module not found";
}
#endif  // !ADDRESS_SANITIZER && !MEMORY_SANITIZER

TEST(ProcessReaderLinux, SelfModules) {
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
  const std::string module_name = "test_module.so";
  const std::string module_soname = "test_module_soname";
  ScopedModuleHandle empty_test_module(
      LoadTestModule(module_name, module_soname));
  ASSERT_TRUE(empty_test_module.valid());
#endif  // !ADDRESS_SANITIZER && !MEMORY_SANITIZER

  FakePtraceConnection connection;
  connection.Initialize(getpid());

  ProcessReaderLinux process_reader;
  ASSERT_TRUE(process_reader.Initialize(&connection));

  ExpectModulesFromSelf(process_reader.Modules());
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
  ExpectTestModule(&process_reader, module_soname);
#endif  // !ADDRESS_SANITIZER && !MEMORY_SANITIZER
}

class ChildModuleTest : public Multiprocess {
 public:
  ChildModuleTest() : Multiprocess(), module_soname_("test_module_soname") {}
  ~ChildModuleTest() = default;

 private:
  void MultiprocessParent() override {
    char c;
    ASSERT_TRUE(LoggingReadFileExactly(ReadPipeHandle(), &c, sizeof(c)));

    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(ChildPID()));

    ProcessReaderLinux process_reader;
    ASSERT_TRUE(process_reader.Initialize(&connection));

    ExpectModulesFromSelf(process_reader.Modules());
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
    ExpectTestModule(&process_reader, module_soname_);
#endif  // !ADDRESS_SANITIZER && !MEMORY_SANITIZER
  }

  void MultiprocessChild() override {
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
    ScopedModuleHandle empty_test_module(
        LoadTestModule("test_module.so", module_soname_));
    ASSERT_TRUE(empty_test_module.valid());
#endif  // !ADDRESS_SANITIZER && !MEMORY_SANITIZER

    char c = 0;
    ASSERT_TRUE(LoggingWriteFile(WritePipeHandle(), &c, sizeof(c)));

    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  const std::string module_soname_;

  DISALLOW_COPY_AND_ASSIGN(ChildModuleTest);
};

TEST(ProcessReaderLinux, ChildModules) {
  ChildModuleTest test;
  test.Run();
}

#if defined(OS_ANDROID)
const char kTestAbortMessage[] = "test abort message";

TEST(ProcessReaderLinux, AbortMessage) {
  // This test requires Q. The API level on Q devices will be 28 until the API
  // is finalized, so we can't check API level yet. For now, test for the
  // presence of a libc symbol which was introduced in Q.
  if (!crashpad::internal::Dlsym(RTLD_DEFAULT,
                                 "android_fdsan_close_with_tag")) {
    GTEST_SKIP();
  }

  android_set_abort_message(kTestAbortMessage);

  FakePtraceConnection connection;
  connection.Initialize(getpid());

  ProcessReaderLinux process_reader;
  ASSERT_TRUE(process_reader.Initialize(&connection));

  EXPECT_EQ(kTestAbortMessage, process_reader.AbortMessage());
}
#endif

}  // namespace
}  // namespace test
}  // namespace crashpad
