// Copyright 2018 The Crashpad Authors
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

#include "snapshot/fuchsia/process_snapshot_fuchsia.h"

#include <dbghelp.h>
#include <zircon/syscalls.h>

#include <iterator>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "snapshot/fuchsia/memory_map_region_snapshot_fuchsia.h"
#include "test/multiprocess_exec.h"
#include "util/fuchsia/koid_utilities.h"
#include "util/fuchsia/scoped_task_suspend.h"

namespace crashpad {
namespace test {
namespace {

constexpr struct {
  uint32_t zircon_perm;
  size_t pages;
  uint32_t minidump_perm;
} kTestMappingPermAndSizes[] = {
    // Zircon doesn't currently allow write-only, execute-only, or
    // write-execute-only, returning ZX_ERR_INVALID_ARGS on map.
    {0, 5, PAGE_NOACCESS},
    {ZX_VM_PERM_READ, 6, PAGE_READONLY},
    // {ZX_VM_PERM_WRITE, 7, PAGE_WRITECOPY},
    // {ZX_VM_PERM_EXECUTE, 8, PAGE_EXECUTE},
    {ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 9, PAGE_READWRITE},
    {ZX_VM_PERM_READ | ZX_VM_PERM_EXECUTE, 10, PAGE_EXECUTE_READ},
    // {ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE, 11, PAGE_EXECUTE_WRITECOPY},
    {ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE,
     12,
     PAGE_EXECUTE_READWRITE},
};

CRASHPAD_CHILD_TEST_MAIN(AddressSpaceChildTestMain) {
  // Create specifically sized mappings/permissions and write the address in
  // our address space to the parent so that the reader can check they're read
  // correctly.
  for (const auto& t : kTestMappingPermAndSizes) {
    zx_handle_t vmo = ZX_HANDLE_INVALID;
    const size_t size = t.pages * zx_system_get_page_size();
    zx_status_t status = zx_vmo_create(size, 0, &vmo);
    ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";
    status = zx_vmo_replace_as_executable(vmo, ZX_HANDLE_INVALID, &vmo);
    ZX_CHECK(status == ZX_OK, status) << "zx_vmo_replace_as_executable";
    uintptr_t mapping_addr = 0;
    status = zx_vmar_map(
        zx_vmar_root_self(), t.zircon_perm, 0, vmo, 0, size, &mapping_addr);
    ZX_CHECK(status == ZX_OK, status) << "zx_vmar_map";
    CheckedWriteFile(StdioFileHandle(StdioStream::kStandardOutput),
                     &mapping_addr,
                     sizeof(mapping_addr));
  }

  CheckedReadFileAtEOF(StdioFileHandle(StdioStream::kStandardInput));
  return 0;
}

bool HasSingleMatchingMapping(
    const std::vector<const MemoryMapRegionSnapshot*>& memory_map,
    uintptr_t address,
    size_t size,
    uint32_t perm) {
  const MemoryMapRegionSnapshot* matching = nullptr;
  for (const auto* region : memory_map) {
    const MINIDUMP_MEMORY_INFO& mmi = region->AsMinidumpMemoryInfo();
    if (mmi.BaseAddress == address) {
      if (matching) {
        LOG(ERROR) << "multiple mappings matching address";
        return false;
      }
      matching = region;
    }
  }

  if (!matching)
    return false;

  const MINIDUMP_MEMORY_INFO& matching_mmi = matching->AsMinidumpMemoryInfo();
  return matching_mmi.Protect == perm && matching_mmi.RegionSize == size;
}

class AddressSpaceTest : public MultiprocessExec {
 public:
  AddressSpaceTest() : MultiprocessExec() {
    SetChildTestMainFunction("AddressSpaceChildTestMain");
  }

  AddressSpaceTest(const AddressSpaceTest&) = delete;
  AddressSpaceTest& operator=(const AddressSpaceTest&) = delete;

  ~AddressSpaceTest() {}

 private:
  void MultiprocessParent() override {
    uintptr_t test_addresses[std::size(kTestMappingPermAndSizes)];
    for (size_t i = 0; i < std::size(test_addresses); ++i) {
      ASSERT_TRUE(ReadFileExactly(
          ReadPipeHandle(), &test_addresses[i], sizeof(test_addresses[i])));
    }

    ScopedTaskSuspend suspend(*ChildProcess());

    ProcessSnapshotFuchsia process_snapshot;
    ASSERT_TRUE(process_snapshot.Initialize(*ChildProcess()));

    for (size_t i = 0; i < std::size(test_addresses); ++i) {
      const auto& t = kTestMappingPermAndSizes[i];
      EXPECT_TRUE(HasSingleMatchingMapping(process_snapshot.MemoryMap(),
                                           test_addresses[i],
                                           t.pages * zx_system_get_page_size(),
                                           t.minidump_perm))
          << base::StringPrintf(
                 "index %zu, zircon_perm 0x%x, minidump_perm 0x%x",
                 i,
                 t.zircon_perm,
                 t.minidump_perm);
    }
  }
};

TEST(ProcessSnapshotFuchsiaTest, AddressSpaceMapping) {
  AddressSpaceTest test;
  test.Run();
}

CRASHPAD_CHILD_TEST_MAIN(StackPointerIntoInvalidLocation) {
  // Map a large block, output the base address of it, and block. The parent
  // will artificially set the SP into this large block to confirm that a huge
  // stack is not accidentally captured.
  zx_handle_t large_vmo;
  constexpr uint64_t kSize = 1 << 30u;
  zx_status_t status = zx_vmo_create(kSize, 0, &large_vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";
  zx_vaddr_t mapped_addr;
  status = zx_vmar_map(zx_vmar_root_self(),
                       ZX_VM_PERM_READ | ZX_VM_PERM_WRITE,
                       0,
                       large_vmo,
                       0,
                       kSize,
                       &mapped_addr);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmar_map";
  CheckedWriteFile(StdioFileHandle(StdioStream::kStandardOutput),
                   &mapped_addr,
                   sizeof(mapped_addr));
  zx_nanosleep(ZX_TIME_INFINITE);
  return 0;
}

class InvalidStackPointerTest : public MultiprocessExec {
 public:
  InvalidStackPointerTest() : MultiprocessExec() {
    SetChildTestMainFunction("StackPointerIntoInvalidLocation");
    SetExpectedChildTermination(kTerminationNormal,
                                ZX_TASK_RETCODE_SYSCALL_KILL);
  }

  InvalidStackPointerTest(const InvalidStackPointerTest&) = delete;
  InvalidStackPointerTest& operator=(const InvalidStackPointerTest&) = delete;

  ~InvalidStackPointerTest() {}

 private:
  void MultiprocessParent() override {
    uint64_t address_of_large_mapping;
    ASSERT_TRUE(ReadFileExactly(ReadPipeHandle(),
                                &address_of_large_mapping,
                                sizeof(address_of_large_mapping)));

    ScopedTaskSuspend suspend(*ChildProcess());

    std::vector<zx::thread> threads = GetThreadHandles(*ChildProcess());
    ASSERT_EQ(threads.size(), 1u);

    zx_thread_state_general_regs_t regs;
    ASSERT_EQ(threads[0].read_state(
                  ZX_THREAD_STATE_GENERAL_REGS, &regs, sizeof(regs)),
              ZX_OK);

    constexpr uint64_t kOffsetIntoMapping = 1024;
#if defined(ARCH_CPU_X86_64)
    regs.rsp = address_of_large_mapping + kOffsetIntoMapping;
#elif defined(ARCH_CPU_ARM64) || defined(ARCH_CPU_RISCV64)
    regs.sp = address_of_large_mapping + kOffsetIntoMapping;
#else
#error
#endif

    ASSERT_EQ(threads[0].write_state(
                  ZX_THREAD_STATE_GENERAL_REGS, &regs, sizeof(regs)),
              ZX_OK);

    ProcessSnapshotFuchsia process_snapshot;
    ASSERT_TRUE(process_snapshot.Initialize(*ChildProcess()));

    ASSERT_EQ(process_snapshot.Threads().size(), 1u);
    const MemorySnapshot* stack = process_snapshot.Threads()[0]->Stack();
    ASSERT_TRUE(stack);
    // Ensure the stack capture isn't unreasonably large.
    EXPECT_LT(stack->Size(), 10 * 1048576u);

    // As we've corrupted the child, don't let it run again.
    ASSERT_EQ(ChildProcess()->kill(), ZX_OK);
  }
};

// This is a test for a specific failure detailed in
// https://bugs.fuchsia.dev/p/fuchsia/issues/detail?id=41212. A test of stack
// behavior that was intentionally overflowing the stack, and so when Crashpad
// received the exception the SP did not point into the actual stack. This
// caused Crashpad to erronously capture the "stack" from the next mapping in
// the address space (which could be very large, cause OOM, etc.).
TEST(ProcessSnapshotFuchsiaTest, InvalidStackPointer) {
  InvalidStackPointerTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
