// Copyright 2017 The Crashpad Authors
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

#include "util/linux/memory_map.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "partition_alloc/buildflags.h"
#include "test/errors.h"
#include "test/file.h"
#include "test/linux/fake_ptrace_connection.h"
#include "test/multiprocess.h"
#include "test/scoped_temp_dir.h"
#include "third_party/lss/lss.h"
#include "util/file/file_io.h"
#include "util/file/scoped_remove_file.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/misc/clock.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/scoped_mmap.h"

namespace crashpad {
namespace test {
namespace {

// TODO(tasak): Disable SelfLargeFiles when PartitionAlloc is used as malloc.
// Because malloc() will cause new mmap() in the case. So while
// reading /proc/self/maps, any memory allocation will update the maps file and
// will cause "format_error". (e.g. GetDelim uses std::string. If std::string
// allocates memory internally (e.g. append and so on), map.Initialize() will
// fail.) To avoid this failue, firstly allocate a large buffer and read entire
// /proc/self/maps into the buffer. Next will parse data from the buffer and
// initialize MemoryMap. crbug.com/1163794.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#define MAYBE_SelfLargeFiles DISABLED_SelfLargeFiles
#else
#define MAYBE_SelfLargeFiles SelfLargeFiles
#endif
TEST(MemoryMap, MAYBE_SelfLargeFiles) {
  // This test is meant to test the handler's ability to understand files
  // mapped from large offsets, even if the handler wasn't built with
  // _FILE_OFFSET_BITS=64. ScopedTempDir needs to stat files to determine
  // whether to recurse into directories, which may will fail without large file
  // support. ScopedRemoveFile doesn't have that restriction.
  ScopedTempDir dir;
  ScopedRemoveFile large_file_path(dir.path().Append("crashpad_test_file"));
  ScopedFileHandle handle(
      LoggingOpenFileForReadAndWrite(large_file_path.get(),
                                     FileWriteMode::kCreateOrFail,
                                     FilePermissions::kWorldReadable));
  ASSERT_TRUE(handle.is_valid());

  // sys_fallocate supports large files as long as the kernel supports them,
  // regardless of _FILE_OFFSET_BITS.
  off64_t off = 1llu + UINT32_MAX;
  ASSERT_EQ(sys_fallocate(handle.get(), 0, off, getpagesize()), 0)
      << ErrnoMessage("fallocate");

  ScopedMmap mapping;
  void* addr = sys_mmap(
      nullptr, getpagesize(), PROT_READ, MAP_SHARED, handle.get(), off);
  ASSERT_TRUE(addr);
  ASSERT_TRUE(mapping.ResetAddrLen(addr, getpagesize()));

  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));
  MemoryMap map;
  ASSERT_TRUE(map.Initialize(&connection));
}

// TODO(tasak): Disable SelfBasic when PartitionAlloc is used as malloc.
// crbug.com/1163794. See SelfLargeFiles' comment.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#define MAYBE_SelfBasic DISABLED_SelfBasic
#else
#define MAYBE_SelfBasic SelfBasic
#endif
TEST(MemoryMap, MAYBE_SelfBasic) {
  ScopedMmap mmapping;
  ASSERT_TRUE(mmapping.ResetMmap(nullptr,
                                 getpagesize(),
                                 PROT_EXEC | PROT_READ,
                                 MAP_SHARED | MAP_ANON,
                                 -1,
                                 0));

  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));

  MemoryMap map;
  ASSERT_TRUE(map.Initialize(&connection));

  auto stack_address = FromPointerCast<LinuxVMAddress>(&map);
  const MemoryMap::Mapping* mapping = map.FindMapping(stack_address);
  ASSERT_TRUE(mapping);
  EXPECT_GE(stack_address, mapping->range.Base());
  EXPECT_LT(stack_address, mapping->range.End());
  EXPECT_TRUE(mapping->readable);
  EXPECT_TRUE(mapping->writable);

  auto code_address = FromPointerCast<LinuxVMAddress>(getpid);
  mapping = map.FindMapping(code_address);
  ASSERT_TRUE(mapping);
  EXPECT_GE(code_address, mapping->range.Base());
  EXPECT_LT(code_address, mapping->range.End());
#if !BUILDFLAG(IS_ANDROID)
  // Android Q+ supports execute only memory.
  EXPECT_TRUE(mapping->readable);
#endif
  EXPECT_FALSE(mapping->writable);
  EXPECT_TRUE(mapping->executable);

  auto mapping_address = mmapping.addr_as<LinuxVMAddress>();
  mapping = map.FindMapping(mapping_address);
  ASSERT_TRUE(mapping);
  mapping = map.FindMapping(mapping_address + mmapping.len() - 1);
  ASSERT_TRUE(mapping);
  EXPECT_EQ(mapping_address, mapping->range.Base());
  EXPECT_EQ(mapping_address + mmapping.len(), mapping->range.End());
  EXPECT_TRUE(mapping->readable);
  EXPECT_FALSE(mapping->writable);
  EXPECT_TRUE(mapping->executable);
  EXPECT_TRUE(mapping->shareable);
}

void InitializeFile(const base::FilePath& path,
                    size_t size,
                    ScopedFileHandle* handle) {
  ASSERT_FALSE(FileExists(path));

  handle->reset(LoggingOpenFileForReadAndWrite(
      path, FileWriteMode::kReuseOrCreate, FilePermissions::kOwnerOnly));
  ASSERT_TRUE(handle->is_valid());
  std::string file_contents(size, std::string::value_type());
  CheckedWriteFile(handle->get(), file_contents.c_str(), file_contents.size());
}

class MapChildTest : public Multiprocess {
 public:
  MapChildTest() : Multiprocess(), page_size_(getpagesize()) {}

  MapChildTest(const MapChildTest&) = delete;
  MapChildTest& operator=(const MapChildTest&) = delete;

  ~MapChildTest() {}

 private:
  void MultiprocessParent() override {
    LinuxVMAddress code_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &code_address, sizeof(code_address));

    LinuxVMAddress stack_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &stack_address, sizeof(stack_address));

    LinuxVMAddress mapped_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &mapped_address, sizeof(mapped_address));

    LinuxVMAddress mapped_file_address;
    CheckedReadFileExactly(
        ReadPipeHandle(), &mapped_file_address, sizeof(mapped_file_address));
    LinuxVMSize path_length;
    CheckedReadFileExactly(ReadPipeHandle(), &path_length, sizeof(path_length));
    std::string mapped_file_name(path_length, std::string::value_type());
    CheckedReadFileExactly(ReadPipeHandle(), &mapped_file_name[0], path_length);

    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(ChildPID()));

    MemoryMap map;
    ASSERT_TRUE(map.Initialize(&connection));

    const MemoryMap::Mapping* mapping = map.FindMapping(code_address);
    ASSERT_TRUE(mapping);
    EXPECT_GE(code_address, mapping->range.Base());
    EXPECT_LT(code_address, mapping->range.End());
#if !BUILDFLAG(IS_ANDROID)
    // Android Q+ supports execute only memory.
    EXPECT_TRUE(mapping->readable);
#endif
    EXPECT_TRUE(mapping->executable);
    EXPECT_FALSE(mapping->writable);

    mapping = map.FindMapping(stack_address);
    ASSERT_TRUE(mapping);
    EXPECT_GE(stack_address, mapping->range.Base());
    EXPECT_LT(stack_address, mapping->range.End());
    EXPECT_TRUE(mapping->readable);
    EXPECT_TRUE(mapping->writable);

    mapping = map.FindMapping(mapped_address);
    ASSERT_TRUE(mapping);
    EXPECT_EQ(mapped_address, mapping->range.Base());
    EXPECT_EQ(mapped_address + page_size_, mapping->range.End());
    EXPECT_FALSE(mapping->readable);
    EXPECT_FALSE(mapping->writable);
    EXPECT_FALSE(mapping->executable);
    EXPECT_TRUE(mapping->shareable);

    mapping = map.FindMapping(mapped_file_address);
    ASSERT_TRUE(mapping);
    EXPECT_EQ(mapped_file_address, mapping->range.Base());
    EXPECT_EQ(mapping->offset, static_cast<int64_t>(page_size_));
    EXPECT_TRUE(mapping->readable);
    EXPECT_TRUE(mapping->writable);
    EXPECT_FALSE(mapping->executable);
    EXPECT_FALSE(mapping->shareable);
    EXPECT_EQ(mapping->name, mapped_file_name);
    struct stat file_stat;
    ASSERT_EQ(stat(mapped_file_name.c_str(), &file_stat), 0)
        << ErrnoMessage("stat");
    EXPECT_EQ(mapping->device, file_stat.st_dev);
    EXPECT_EQ(mapping->inode, file_stat.st_ino);
    EXPECT_EQ(map.FindMappingWithName(mapping->name), mapping);
  }

  void MultiprocessChild() override {
    auto code_address = FromPointerCast<LinuxVMAddress>(getpid);
    CheckedWriteFile(WritePipeHandle(), &code_address, sizeof(code_address));

    auto stack_address = FromPointerCast<LinuxVMAddress>(&code_address);
    CheckedWriteFile(WritePipeHandle(), &stack_address, sizeof(stack_address));

    ScopedMmap mapping;
    ASSERT_TRUE(mapping.ResetMmap(
        nullptr, page_size_, PROT_NONE, MAP_SHARED | MAP_ANON, -1, 0));
    auto mapped_address = mapping.addr_as<LinuxVMAddress>();
    CheckedWriteFile(
        WritePipeHandle(), &mapped_address, sizeof(mapped_address));

    ScopedTempDir temp_dir;
    base::FilePath path =
        temp_dir.path().Append(FILE_PATH_LITERAL("test_file"));
    ScopedFileHandle handle;
    ASSERT_NO_FATAL_FAILURE(InitializeFile(path, page_size_ * 2, &handle));

    ScopedMmap file_mapping;
    ASSERT_TRUE(file_mapping.ResetMmap(nullptr,
                                       page_size_,
                                       PROT_READ | PROT_WRITE,
                                       MAP_PRIVATE,
                                       handle.get(),
                                       page_size_));

    auto mapped_file_address = file_mapping.addr_as<LinuxVMAddress>();
    CheckedWriteFile(
        WritePipeHandle(), &mapped_file_address, sizeof(mapped_file_address));
    LinuxVMSize path_length = path.value().size();
    CheckedWriteFile(WritePipeHandle(), &path_length, sizeof(path_length));
    CheckedWriteFile(WritePipeHandle(), path.value().c_str(), path_length);

    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  const size_t page_size_;
};

TEST(MemoryMap, MapChild) {
  MapChildTest test;
  test.Run();
}

// Some systems optimize mappings by allocating new mappings inside existing
// mappings with matching permissions. Work around this by allocating one large
// mapping and then switching up the permissions of individual pages to force
// populating more entries in the maps file.
void InitializeMappings(ScopedMmap* mappings,
                        size_t num_mappings,
                        size_t mapping_size) {
  ASSERT_TRUE(mappings->ResetMmap(nullptr,
                                  mapping_size * num_mappings,
                                  PROT_READ,
                                  MAP_PRIVATE | MAP_ANON,
                                  -1,
                                  0));

  auto region = mappings->addr_as<LinuxVMAddress>();
  for (size_t index = 0; index < num_mappings; index += 2) {
    ASSERT_EQ(mprotect(reinterpret_cast<void*>(region + index * mapping_size),
                       mapping_size,
                       PROT_READ | PROT_WRITE),
              0)
        << ErrnoMessage("mprotect");
  }
}

void ExpectMappings(const MemoryMap& map,
                    LinuxVMAddress region_addr,
                    size_t num_mappings,
                    size_t mapping_size) {
  for (size_t index = 0; index < num_mappings; ++index) {
    SCOPED_TRACE(base::StringPrintf("index %zu", index));

    auto mapping_address = region_addr + index * mapping_size;
    const MemoryMap::Mapping* mapping = map.FindMapping(mapping_address);
    ASSERT_TRUE(mapping);
    EXPECT_EQ(mapping_address, mapping->range.Base());
    EXPECT_EQ(mapping_address + mapping_size, mapping->range.End());
    EXPECT_TRUE(mapping->readable);
    EXPECT_FALSE(mapping->shareable);
    EXPECT_FALSE(mapping->executable);
    if (index % 2 == 0) {
      EXPECT_TRUE(mapping->writable);
    } else {
      EXPECT_FALSE(mapping->writable);
    }
  }
}

// TODO(tasak): Disable SelfLargeMapFile when PartitionAlloc is used as malloc.
// crbug.com/1163794. See SelfLargeFiles' comment.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#define MAYBE_SelfLargeMapFile DISABLED_SelfLargeMapFile
#else
#define MAYBE_SelfLargeMapFile SelfLargeMapFile
#endif
TEST(MemoryMap, MAYBE_SelfLargeMapFile) {
  constexpr size_t kNumMappings = 1024;
  const size_t page_size = getpagesize();
  ScopedMmap mappings;

  ASSERT_NO_FATAL_FAILURE(
      InitializeMappings(&mappings, kNumMappings, page_size));

  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));

  MemoryMap map;
  ASSERT_TRUE(map.Initialize(&connection));

  ExpectMappings(
      map, mappings.addr_as<LinuxVMAddress>(), kNumMappings, page_size);
}

class MapRunningChildTest : public Multiprocess {
 public:
  MapRunningChildTest() : Multiprocess(), page_size_(getpagesize()) {}

  MapRunningChildTest(const MapRunningChildTest&) = delete;
  MapRunningChildTest& operator=(const MapRunningChildTest&) = delete;

  ~MapRunningChildTest() {}

 private:
  void MultiprocessParent() override {
    // Let the child get started
    LinuxVMAddress region_addr;
    CheckedReadFileExactly(ReadPipeHandle(), &region_addr, sizeof(region_addr));

    for (int iter = 0; iter < 8; ++iter) {
      SCOPED_TRACE(base::StringPrintf("iter %d", iter));

      // Let the child get back to its work
      SleepNanoseconds(1000);

      DirectPtraceConnection connection;
      ASSERT_TRUE(connection.Initialize(ChildPID()));

      MemoryMap map;
      ASSERT_TRUE(map.Initialize(&connection));

      // We should at least find the original mappings. The extra mappings may
      // or not be found depending on scheduling.
      ExpectMappings(map, region_addr, kNumMappings, page_size_);
    }
  }

  void MultiprocessChild() override {
    ASSERT_EQ(fcntl(ReadPipeHandle(), F_SETFL, O_NONBLOCK), 0)
        << ErrnoMessage("fcntl");

    ScopedMmap mappings;
    ASSERT_NO_FATAL_FAILURE(
        InitializeMappings(&mappings, kNumMappings, page_size_));

    // Let the parent start mapping us
    auto region_addr = mappings.addr_as<LinuxVMAddress>();
    CheckedWriteFile(WritePipeHandle(), &region_addr, sizeof(region_addr));

    // But don't stop there!
    constexpr size_t kNumExtraMappings = 256;
    ScopedMmap extra_mappings;

    while (true) {
      ASSERT_NO_FATAL_FAILURE(
          InitializeMappings(&extra_mappings, kNumExtraMappings, page_size_));

      // Quit when the parent is done
      char c;
      FileOperationResult res = ReadFile(ReadPipeHandle(), &c, sizeof(c));
      if (res == 0) {
        break;
      }
      ASSERT_EQ(errno, EAGAIN);
    }
  }

  static constexpr size_t kNumMappings = 1024;
  const size_t page_size_;
};

TEST(MemoryMap, MapRunningChild) {
  MapRunningChildTest test;
  test.Run();
}

// Expects first and third pages from mapping_start to refer to the same mapped
// file. The second page should not.
void ExpectFindFilePossibleMmapStarts(LinuxVMAddress mapping_start,
                                      LinuxVMSize page_size) {
  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));

  MemoryMap map;
  ASSERT_TRUE(map.Initialize(&connection));

  auto mapping1 = map.FindMapping(mapping_start);
  ASSERT_TRUE(mapping1);
  auto mapping2 = map.FindMapping(mapping_start + page_size);
  ASSERT_TRUE(mapping2);
  auto mapping3 = map.FindMapping(mapping_start + page_size * 2);
  ASSERT_TRUE(mapping3);

  ASSERT_NE(mapping1, mapping2);
  ASSERT_NE(mapping2, mapping3);

  auto mappings = map.FindFilePossibleMmapStarts(*mapping1);
  ASSERT_EQ(mappings->Count(), 1u);
  EXPECT_EQ(mappings->Next(), mapping1);

  mappings = map.FindFilePossibleMmapStarts(*mapping2);
  ASSERT_EQ(mappings->Count(), 1u);
  EXPECT_EQ(mappings->Next(), mapping2);

  mappings = map.FindFilePossibleMmapStarts(*mapping3);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(mappings->Count(), 2u);
#else
  ASSERT_EQ(mappings->Count(), 1u);
  EXPECT_EQ(mappings->Next(), mapping1);
#endif
}

TEST(MemoryMap, FindFilePossibleMmapStarts) {
  const size_t page_size = getpagesize();

  ScopedTempDir temp_dir;
  base::FilePath path = temp_dir.path().Append(
      FILE_PATH_LITERAL("FindFilePossibleMmapStartsTestFile"));
  ScopedFileHandle handle;
  size_t file_length = page_size * 3;
  ASSERT_NO_FATAL_FAILURE(InitializeFile(path, file_length, &handle));

  ScopedMmap file_mapping;
  ASSERT_TRUE(file_mapping.ResetMmap(
      nullptr, file_length, PROT_READ, MAP_PRIVATE, handle.get(), 0));
  auto mapping_start = file_mapping.addr_as<LinuxVMAddress>();

  // Change the permissions on the second page to split the mapping into three
  // parts.
  ASSERT_EQ(mprotect(file_mapping.addr_as<char*>() + page_size,
                     page_size,
                     PROT_READ | PROT_WRITE),
            0);

  // Basic
  {
    FakePtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(getpid()));

    MemoryMap map;
    ASSERT_TRUE(map.Initialize(&connection));

    auto mapping1 = map.FindMapping(mapping_start);
    ASSERT_TRUE(mapping1);
    auto mapping2 = map.FindMapping(mapping_start + page_size);
    ASSERT_TRUE(mapping2);
    auto mapping3 = map.FindMapping(mapping_start + page_size * 2);
    ASSERT_TRUE(mapping3);

    ASSERT_NE(mapping1, mapping2);
    ASSERT_NE(mapping2, mapping3);

#if BUILDFLAG(IS_ANDROID)
    auto mappings = map.FindFilePossibleMmapStarts(*mapping1);
    EXPECT_EQ(mappings->Count(), 1u);
    EXPECT_EQ(mappings->Next(), mapping1);
    EXPECT_EQ(mappings->Next(), nullptr);

    mappings = map.FindFilePossibleMmapStarts(*mapping2);
    EXPECT_EQ(mappings->Count(), 2u);

    mappings = map.FindFilePossibleMmapStarts(*mapping3);
    EXPECT_EQ(mappings->Count(), 3u);
#else
    auto mappings = map.FindFilePossibleMmapStarts(*mapping1);
    ASSERT_EQ(mappings->Count(), 1u);
    EXPECT_EQ(mappings->Next(), mapping1);
    EXPECT_EQ(mappings->Next(), nullptr);

    mappings = map.FindFilePossibleMmapStarts(*mapping2);
    ASSERT_EQ(mappings->Count(), 1u);
    EXPECT_EQ(mappings->Next(), mapping1);

    mappings = map.FindFilePossibleMmapStarts(*mapping3);
    ASSERT_EQ(mappings->Count(), 1u);
    EXPECT_EQ(mappings->Next(), mapping1);
#endif

#if defined(ARCH_CPU_64_BITS)
    constexpr bool is_64_bit = true;
#else
    constexpr bool is_64_bit = false;
#endif
    MemoryMap::Mapping bad_mapping;
    bad_mapping.range.SetRange(is_64_bit, 0, 1);
    mappings = map.FindFilePossibleMmapStarts(bad_mapping);
    EXPECT_EQ(mappings->Count(), 0u);
    EXPECT_EQ(mappings->Next(), nullptr);
  }

  // Make the second page an anonymous mapping
  file_mapping.ResetAddrLen(file_mapping.addr_as<void*>(), page_size);
  ScopedMmap page2_mapping;
  ASSERT_TRUE(page2_mapping.ResetMmap(file_mapping.addr_as<char*>() + page_size,
                                      page_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANON | MAP_FIXED,
                                      -1,
                                      0));
  ScopedMmap page3_mapping;
  ASSERT_TRUE(
      page3_mapping.ResetMmap(file_mapping.addr_as<char*>() + page_size * 2,
                              page_size,
                              PROT_READ,
                              MAP_PRIVATE | MAP_FIXED,
                              handle.get(),
                              page_size * 2));
  ExpectFindFilePossibleMmapStarts(mapping_start, page_size);

  // Map the second page to another file.
  ScopedFileHandle handle2;
  base::FilePath path2 = temp_dir.path().Append(
      FILE_PATH_LITERAL("FindFilePossibleMmapStartsTestFile2"));
  ASSERT_NO_FATAL_FAILURE(InitializeFile(path2, page_size, &handle2));

  page2_mapping.ResetMmap(file_mapping.addr_as<char*>() + page_size,
                          page_size,
                          PROT_READ,
                          MAP_PRIVATE | MAP_FIXED,
                          handle2.get(),
                          0);
  ExpectFindFilePossibleMmapStarts(mapping_start, page_size);
}

TEST(MemoryMap, FindFilePossibleMmapStarts_MultipleStarts) {
  ScopedTempDir temp_dir;
  base::FilePath path =
      temp_dir.path().Append(FILE_PATH_LITERAL("MultipleStartsTestFile"));
  const size_t page_size = getpagesize();
  ScopedFileHandle handle;
  ASSERT_NO_FATAL_FAILURE(InitializeFile(path, page_size * 2, &handle));

  // Locate a sequence of pages to setup a test in.
  char* seq_addr;
  {
    ScopedMmap whole_mapping;
    ASSERT_TRUE(whole_mapping.ResetMmap(
        nullptr, page_size * 8, PROT_READ, MAP_PRIVATE | MAP_ANON, -1, 0));
    seq_addr = whole_mapping.addr_as<char*>();
  }

  // Arrange file and anonymous mappings in the sequence.
  ScopedMmap file_mapping0;
  ASSERT_TRUE(file_mapping0.ResetMmap(seq_addr,
                                      page_size,
                                      PROT_READ,
                                      MAP_PRIVATE | MAP_FIXED,
                                      handle.get(),
                                      page_size));

  ScopedMmap file_mapping1;
  ASSERT_TRUE(file_mapping1.ResetMmap(seq_addr + page_size,
                                      page_size * 2,
                                      PROT_READ,
                                      MAP_PRIVATE | MAP_FIXED,
                                      handle.get(),
                                      0));

  ScopedMmap file_mapping2;
  ASSERT_TRUE(file_mapping2.ResetMmap(seq_addr + page_size * 3,
                                      page_size,
                                      PROT_READ,
                                      MAP_PRIVATE | MAP_FIXED,
                                      handle.get(),
                                      0));

  // Skip a page

  ScopedMmap file_mapping3;
  ASSERT_TRUE(file_mapping3.ResetMmap(seq_addr + page_size * 5,
                                      page_size,
                                      PROT_READ,
                                      MAP_PRIVATE | MAP_FIXED,
                                      handle.get(),
                                      0));

  ScopedMmap anon_mapping;
  ASSERT_TRUE(anon_mapping.ResetMmap(seq_addr + page_size * 6,
                                     page_size,
                                     PROT_READ,
                                     MAP_PRIVATE | MAP_ANON | MAP_FIXED,
                                     -1,
                                     0));

  ScopedMmap file_mapping4;
  ASSERT_TRUE(file_mapping4.ResetMmap(seq_addr + page_size * 7,
                                      page_size,
                                      PROT_READ,
                                      MAP_PRIVATE | MAP_FIXED,
                                      handle.get(),
                                      0));

  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));
  MemoryMap map;
  ASSERT_TRUE(map.Initialize(&connection));

  auto mapping = map.FindMapping(file_mapping0.addr_as<VMAddress>());
  ASSERT_TRUE(mapping);
  auto possible_starts = map.FindFilePossibleMmapStarts(*mapping);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(possible_starts->Count(), 1u);
#else
  EXPECT_EQ(possible_starts->Count(), 0u);
#endif

  mapping = map.FindMapping(file_mapping1.addr_as<VMAddress>());
  ASSERT_TRUE(mapping);
  possible_starts = map.FindFilePossibleMmapStarts(*mapping);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(possible_starts->Count(), 2u);
#else
  EXPECT_EQ(possible_starts->Count(), 1u);
#endif

  mapping = map.FindMapping(file_mapping2.addr_as<VMAddress>());
  ASSERT_TRUE(mapping);
  possible_starts = map.FindFilePossibleMmapStarts(*mapping);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(possible_starts->Count(), 3u);
#else
  EXPECT_EQ(possible_starts->Count(), 2u);
#endif

  mapping = map.FindMapping(file_mapping3.addr_as<VMAddress>());
  ASSERT_TRUE(mapping);
  possible_starts = map.FindFilePossibleMmapStarts(*mapping);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(possible_starts->Count(), 4u);
#else
  EXPECT_EQ(possible_starts->Count(), 3u);
#endif

  mapping = map.FindMapping(file_mapping4.addr_as<VMAddress>());
  ASSERT_TRUE(mapping);
  possible_starts = map.FindFilePossibleMmapStarts(*mapping);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(possible_starts->Count(), 5u);
#else
  EXPECT_EQ(possible_starts->Count(), 4u);
#endif
}

}  // namespace
}  // namespace test
}  // namespace crashpad
