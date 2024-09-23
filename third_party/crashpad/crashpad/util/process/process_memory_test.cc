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

#include "util/process/process_memory.h"

#include <string.h>

#include "base/containers/heap_array.h"
#include "base/memory/page_size.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "test/multiprocess_exec.h"
#include "test/process_type.h"
#include "test/scoped_guarded_page.h"
#include "util/file/file_io.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_native.h"

#if BUILDFLAG(IS_APPLE)
#include "test/mac/mach_multiprocess.h"
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "test/linux/fake_ptrace_connection.h"
#include "util/linux/direct_ptrace_connection.h"
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

namespace crashpad {
namespace test {
namespace {

// On macOS the ProcessMemoryTests require accessing the child process' task
// port which requires root or a code signing entitlement. To account for this
// we implement an adaptor class that wraps MachMultiprocess on macOS, because
// it shares the child's task port, and makes it behave like MultiprocessExec.
#if BUILDFLAG(IS_APPLE)
class MultiprocessAdaptor : public MachMultiprocess {
 public:
  void SetChildTestMainFunction(const std::string& function_name) {
    test_function_ = function_name;
  }

  ProcessType ChildProcess() { return ChildTask(); }

  // Helpers to get I/O handles in the child process
  static FileHandle OutputHandle() {
    CHECK_NE(write_pipe_handle_, -1);
    return write_pipe_handle_;
  }

  static FileHandle InputHandle() {
    CHECK_NE(read_pipe_handle_, -1);
    return read_pipe_handle_;
  }

 private:
  virtual void Parent() = 0;

  void MachMultiprocessParent() override { Parent(); }

  void MachMultiprocessChild() override {
    read_pipe_handle_ = ReadPipeHandle();
    write_pipe_handle_ = WritePipeHandle();
    internal::CheckedInvokeMultiprocessChild(test_function_);
  }

  std::string test_function_;

  static FileHandle read_pipe_handle_;
  static FileHandle write_pipe_handle_;
};

FileHandle MultiprocessAdaptor::read_pipe_handle_ = -1;
FileHandle MultiprocessAdaptor::write_pipe_handle_ = -1;
#else
class MultiprocessAdaptor : public MultiprocessExec {
 public:
  static FileHandle OutputHandle() {
    return StdioFileHandle(StdioStream::kStandardOutput);
  }

  static FileHandle InputHandle() {
    return StdioFileHandle(StdioStream::kStandardInput);
  }

 private:
  virtual void Parent() = 0;

  void MultiprocessParent() override { Parent(); }
};
#endif  // BUILDFLAG(IS_APPLE)

base::HeapArray<char> DoChildReadTestSetup() {
  auto region = base::HeapArray<char>::Uninit(4 * base::GetPageSize());
  for (size_t index = 0; index < region.size(); ++index) {
    region[index] = static_cast<char>(index % 256);
  }
  return region;
}

CRASHPAD_CHILD_TEST_MAIN(ReadTestChild) {
  auto region = DoChildReadTestSetup();
  auto region_size = region.size();
  FileHandle out = MultiprocessAdaptor::OutputHandle();
  CheckedWriteFile(out, &region_size, sizeof(region_size));
  VMAddress address = FromPointerCast<VMAddress>(region.data());
  CheckedWriteFile(out, &address, sizeof(address));
  CheckedReadFileAtEOF(MultiprocessAdaptor::InputHandle());
  return 0;
}

class ReadTest : public MultiprocessAdaptor {
 public:
  ReadTest() : MultiprocessAdaptor() {
    SetChildTestMainFunction("ReadTestChild");
  }

  ReadTest(const ReadTest&) = delete;
  ReadTest& operator=(const ReadTest&) = delete;

  void RunAgainstSelf() {
    auto region = DoChildReadTestSetup();
    DoTest(GetSelfProcess(),
           region.size(),
           FromPointerCast<VMAddress>(region.data()));
  }

  void RunAgainstChild() { Run(); }

 private:
  void Parent() override {
    size_t region_size;
    VMAddress region;
    ASSERT_TRUE(
        ReadFileExactly(ReadPipeHandle(), &region_size, sizeof(region_size)));
    ASSERT_TRUE(ReadFileExactly(ReadPipeHandle(), &region, sizeof(region)));
    DoTest(ChildProcess(), region_size, region);
  }

  void DoTest(ProcessType process, size_t region_size, VMAddress address) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    FakePtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(process));
    ProcessMemoryLinux memory(&connection);
#else
    ProcessMemoryNative memory;
    ASSERT_TRUE(memory.Initialize(process));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

    auto result = base::HeapArray<char>::Uninit(region_size);

    // Ensure that the entire region can be read.
    ASSERT_TRUE(memory.Read(address, result.size(), result.data()));
    for (size_t i = 0; i < result.size(); ++i) {
      EXPECT_EQ(result[i], static_cast<char>(i % 256));
    }

    // Ensure that a read of length 0 succeeds and doesn’t touch the result.
    memset(result.data(), '\0', result.size());
    ASSERT_TRUE(memory.Read(address, 0, result.data()));
    for (size_t i = 0; i < result.size(); ++i) {
      EXPECT_EQ(result[i], 0);
    }

    // Ensure that a read starting at an unaligned address works.
    ASSERT_TRUE(memory.Read(address + 1, result.size() - 1, result.data()));
    for (size_t i = 0; i < result.size() - 1; ++i) {
      EXPECT_EQ(result[i], static_cast<char>((i + 1) % 256));
    }

    // Ensure that a read ending at an unaligned address works.
    ASSERT_TRUE(memory.Read(address, result.size() - 1, result.data()));
    for (size_t i = 0; i < result.size() - 1; ++i) {
      EXPECT_EQ(result[i], static_cast<char>(i % 256));
    }

    // Ensure that a read starting and ending at unaligned addresses works.
    ASSERT_TRUE(memory.Read(address + 1, result.size() - 2, result.data()));
    for (size_t i = 0; i < result.size() - 2; ++i) {
      EXPECT_EQ(result[i], static_cast<char>((i + 1) % 256));
    }

    // Ensure that a read of exactly one page works.
    size_t page_size = base::GetPageSize();
    ASSERT_GE(result.size(), page_size + page_size);
    ASSERT_TRUE(memory.Read(address + page_size, page_size, result.data()));
    for (size_t i = 0; i < page_size; ++i) {
      EXPECT_EQ(result[i], static_cast<char>((i + page_size) % 256));
    }

    // Ensure that reading exactly a single byte works.
    result[1] = 'J';
    ASSERT_TRUE(memory.Read(address + 2, 1, result.data()));
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 'J');
  }
};

TEST(ProcessMemory, ReadSelf) {
  ReadTest test;
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadChild) {
  ReadTest test;
  test.RunAgainstChild();
}

constexpr char kConstCharEmpty[] = "";
constexpr char kConstCharShort[] = "A short const char[]";

#define SHORT_LOCAL_STRING "A short local variable char[]"

std::string MakeLongString() {
  std::string long_string;
  const size_t kStringLongSize = 4 * base::GetPageSize();
  for (size_t index = 0; index < kStringLongSize; ++index) {
    long_string.push_back((index % 255) + 1);
  }
  EXPECT_EQ(long_string.size(), kStringLongSize);
  return long_string;
}

void DoChildCStringReadTestSetup(const char** const_empty,
                                 const char** const_short,
                                 const char** local_empty,
                                 const char** local_short,
                                 std::string* long_string) {
  *const_empty = kConstCharEmpty;
  *const_short = kConstCharShort;
  *local_empty = "";
  *local_short = SHORT_LOCAL_STRING;
  *long_string = MakeLongString();
}

CRASHPAD_CHILD_TEST_MAIN(ReadCStringTestChild) {
  const char* const_empty;
  const char* const_short;
  const char* local_empty;
  const char* local_short;
  std::string long_string;
  DoChildCStringReadTestSetup(
      &const_empty, &const_short, &local_empty, &local_short, &long_string);
  const auto write_address = [](const char* p) {
    VMAddress address = FromPointerCast<VMAddress>(p);
    CheckedWriteFile(
        MultiprocessAdaptor::OutputHandle(), &address, sizeof(address));
  };
  write_address(const_empty);
  write_address(const_short);
  write_address(local_empty);
  write_address(local_short);
  write_address(long_string.c_str());
  CheckedReadFileAtEOF(MultiprocessAdaptor::InputHandle());
  return 0;
}

class ReadCStringTest : public MultiprocessAdaptor {
 public:
  ReadCStringTest(bool limit_size)
      : MultiprocessAdaptor(), limit_size_(limit_size) {
    SetChildTestMainFunction("ReadCStringTestChild");
  }

  ReadCStringTest(const ReadCStringTest&) = delete;
  ReadCStringTest& operator=(const ReadCStringTest&) = delete;

  void RunAgainstSelf() {
    const char* const_empty;
    const char* const_short;
    const char* local_empty;
    const char* local_short;
    std::string long_string;
    DoChildCStringReadTestSetup(
        &const_empty, &const_short, &local_empty, &local_short, &long_string);
    DoTest(GetSelfProcess(),
           FromPointerCast<VMAddress>(const_empty),
           FromPointerCast<VMAddress>(const_short),
           FromPointerCast<VMAddress>(local_empty),
           FromPointerCast<VMAddress>(local_short),
           FromPointerCast<VMAddress>(long_string.c_str()));
  }
  void RunAgainstChild() { Run(); }

 private:
  void Parent() override {
#define DECLARE_AND_READ_ADDRESS(name) \
  VMAddress name;                      \
  ASSERT_TRUE(ReadFileExactly(ReadPipeHandle(), &name, sizeof(name)));
    DECLARE_AND_READ_ADDRESS(const_empty_address);
    DECLARE_AND_READ_ADDRESS(const_short_address);
    DECLARE_AND_READ_ADDRESS(local_empty_address);
    DECLARE_AND_READ_ADDRESS(local_short_address);
    DECLARE_AND_READ_ADDRESS(long_string_address);
#undef DECLARE_AND_READ_ADDRESS

    DoTest(ChildProcess(),
           const_empty_address,
           const_short_address,
           local_empty_address,
           local_short_address,
           long_string_address);
  }

  void Compare(ProcessMemory& memory, VMAddress address, const char* str) {
    std::string result;
    if (limit_size_) {
      ASSERT_TRUE(
          memory.ReadCStringSizeLimited(address, strlen(str) + 1, &result));
      EXPECT_EQ(result, str);
      ASSERT_TRUE(
          memory.ReadCStringSizeLimited(address, strlen(str) + 2, &result));
      EXPECT_EQ(result, str);
      EXPECT_FALSE(
          memory.ReadCStringSizeLimited(address, strlen(str), &result));
    } else {
      ASSERT_TRUE(memory.ReadCString(address, &result));
      EXPECT_EQ(result, str);
    }
  }

  void DoTest(ProcessType process,
              VMAddress const_empty_address,
              VMAddress const_short_address,
              VMAddress local_empty_address,
              VMAddress local_short_address,
              VMAddress long_string_address) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    FakePtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(process));
    ProcessMemoryLinux memory(&connection);
#else
    ProcessMemoryNative memory;
    ASSERT_TRUE(memory.Initialize(process));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

    Compare(memory, const_empty_address, kConstCharEmpty);
    Compare(memory, const_short_address, kConstCharShort);
    Compare(memory, local_empty_address, "");
    Compare(memory, local_short_address, SHORT_LOCAL_STRING);
    std::string long_string_for_comparison = MakeLongString();
    Compare(memory, long_string_address, long_string_for_comparison.c_str());
  }

  const bool limit_size_;
};

TEST(ProcessMemory, ReadCStringSelf) {
  ReadCStringTest test(/* limit_size= */ false);
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadCStringChild) {
  ReadCStringTest test(/* limit_size= */ false);
  test.RunAgainstChild();
}

TEST(ProcessMemory, ReadCStringSizeLimitedSelf) {
  ReadCStringTest test(/* limit_size= */ true);
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadCStringSizeLimitedChild) {
  ReadCStringTest test(/* limit_size= */ true);
  test.RunAgainstChild();
}

void DoReadUnmappedChildMainSetup(void* page) {
  char* region = reinterpret_cast<char*>(page);
  for (size_t index = 0; index < base::GetPageSize(); ++index) {
    region[index] = static_cast<char>(index % 256);
  }
}

CRASHPAD_CHILD_TEST_MAIN(ReadUnmappedChildMain) {
  ScopedGuardedPage pages;
  VMAddress address = reinterpret_cast<VMAddress>(pages.Pointer());
  DoReadUnmappedChildMainSetup(pages.Pointer());
  FileHandle out = MultiprocessAdaptor::OutputHandle();
  CheckedWriteFile(out, &address, sizeof(address));
  CheckedReadFileAtEOF(MultiprocessAdaptor::InputHandle());
  return 0;
}

// This test only supports running against a child process because
// ScopedGuardedPage is not thread-safe.
class ReadUnmappedTest : public MultiprocessAdaptor {
 public:
  ReadUnmappedTest() : MultiprocessAdaptor() {
    SetChildTestMainFunction("ReadUnmappedChildMain");
  }

  ReadUnmappedTest(const ReadUnmappedTest&) = delete;
  ReadUnmappedTest& operator=(const ReadUnmappedTest&) = delete;

  void RunAgainstChild() { Run(); }

 private:
  void Parent() override {
    VMAddress address = 0;
    ASSERT_TRUE(ReadFileExactly(ReadPipeHandle(), &address, sizeof(address)));
    DoTest(ChildProcess(), address);
  }

  void DoTest(ProcessType process, VMAddress address) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(process));
    ProcessMemoryLinux memory(&connection);
#else
    ProcessMemoryNative memory;
    ASSERT_TRUE(memory.Initialize(process));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

    VMAddress page_addr1 = address;
    VMAddress page_addr2 = page_addr1 + base::GetPageSize();

    auto result = base::HeapArray<char>::Uninit(base::GetPageSize() * 2);
    EXPECT_TRUE(memory.Read(page_addr1, base::GetPageSize(), result.data()));
    EXPECT_TRUE(memory.Read(page_addr2 - 1, 1, result.data()));

    EXPECT_FALSE(memory.Read(page_addr1, result.size(), result.data()));
    EXPECT_FALSE(memory.Read(page_addr2, base::GetPageSize(), result.data()));
    EXPECT_FALSE(memory.Read(page_addr2 - 1, 2, result.data()));
  }
};

TEST(ProcessMemory, ReadUnmappedChild) {
  ReadUnmappedTest test;
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstChild();
}

constexpr size_t kChildProcessStringLength = 10;

class StringDataInChildProcess {
 public:
  // This constructor only makes sense in the child process.
  explicit StringDataInChildProcess(const char* cstring, bool valid)
      : address_(FromPointerCast<VMAddress>(cstring)) {
    if (valid) {
      memcpy(expected_value_, cstring, kChildProcessStringLength + 1);
    } else {
      memset(expected_value_, 0xff, kChildProcessStringLength + 1);
    }
  }

  void Write(FileHandle out) {
    CheckedWriteFile(out, &address_, sizeof(address_));
    CheckedWriteFile(out, &expected_value_, sizeof(expected_value_));
  }

  static StringDataInChildProcess Read(FileHandle in) {
    StringDataInChildProcess str;
    EXPECT_TRUE(ReadFileExactly(in, &str.address_, sizeof(str.address_)));
    EXPECT_TRUE(
        ReadFileExactly(in, &str.expected_value_, sizeof(str.expected_value_)));
    return str;
  }

  VMAddress address() const { return address_; }
  std::string expected_value() const { return expected_value_; }

  private:
   StringDataInChildProcess() : address_(0), expected_value_() {}

   VMAddress address_;
   char expected_value_[kChildProcessStringLength + 1];
};

void DoCStringUnmappedTestSetup(
    void* page,
    std::vector<StringDataInChildProcess>* strings) {
  char* region = reinterpret_cast<char*>(page);
  for (size_t index = 0; index < base::GetPageSize(); ++index) {
    region[index] = 1 + index % 255;
  }

  // A string at the start of the mapped region
  char* string1 = region;
  string1[kChildProcessStringLength] = '\0';

  // A string near the end of the mapped region
  char* string2 = region + base::GetPageSize() - kChildProcessStringLength * 2;
  string2[kChildProcessStringLength] = '\0';

  // A string that crosses from the mapped into the unmapped region
  char* string3 = region + base::GetPageSize() - kChildProcessStringLength + 1;

  // A string entirely in the unmapped region
  char* string4 = region + base::GetPageSize() + 10;

  strings->push_back(StringDataInChildProcess(string1, true));
  strings->push_back(StringDataInChildProcess(string2, true));
  strings->push_back(StringDataInChildProcess(string3, false));
  strings->push_back(StringDataInChildProcess(string4, false));
}

CRASHPAD_CHILD_TEST_MAIN(ReadCStringUnmappedChildMain) {
  ScopedGuardedPage pages;
  std::vector<StringDataInChildProcess> strings;
  DoCStringUnmappedTestSetup(pages.Pointer(), &strings);
  FileHandle out = MultiprocessAdaptor::OutputHandle();
  strings[0].Write(out);
  strings[1].Write(out);
  strings[2].Write(out);
  strings[3].Write(out);
  CheckedReadFileAtEOF(MultiprocessAdaptor::InputHandle());
  return 0;
}

// This test only supports running against a child process because
// ScopedGuardedPage is not thread-safe.
class ReadCStringUnmappedTest : public MultiprocessAdaptor {
 public:
  ReadCStringUnmappedTest(bool limit_size)
      : MultiprocessAdaptor(), limit_size_(limit_size) {
    SetChildTestMainFunction("ReadCStringUnmappedChildMain");
  }

  ReadCStringUnmappedTest(const ReadCStringUnmappedTest&) = delete;
  ReadCStringUnmappedTest& operator=(const ReadCStringUnmappedTest&) = delete;

  void RunAgainstChild() { Run(); }

 private:
  void Parent() override {
    std::vector<StringDataInChildProcess> strings;
    strings.push_back(StringDataInChildProcess::Read(ReadPipeHandle()));
    strings.push_back(StringDataInChildProcess::Read(ReadPipeHandle()));
    strings.push_back(StringDataInChildProcess::Read(ReadPipeHandle()));
    strings.push_back(StringDataInChildProcess::Read(ReadPipeHandle()));
    ASSERT_NO_FATAL_FAILURE(DoTest(ChildProcess(), strings));
  }

  void DoTest(ProcessType process,
              const std::vector<StringDataInChildProcess>& strings) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(process));
    ProcessMemoryLinux memory(&connection);
#else
    ProcessMemoryNative memory;
    ASSERT_TRUE(memory.Initialize(process));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

    std::string result;
    result.reserve(kChildProcessStringLength + 1);

    if (limit_size_) {
      ASSERT_TRUE(memory.ReadCStringSizeLimited(
          strings[0].address(), kChildProcessStringLength + 1, &result));
      EXPECT_EQ(result, strings[0].expected_value());
      ASSERT_TRUE(memory.ReadCStringSizeLimited(
          strings[1].address(), kChildProcessStringLength + 1, &result));
      EXPECT_EQ(result, strings[1].expected_value());
      EXPECT_FALSE(memory.ReadCStringSizeLimited(
          strings[2].address(), kChildProcessStringLength + 1, &result));
      EXPECT_FALSE(memory.ReadCStringSizeLimited(
          strings[3].address(), kChildProcessStringLength + 1, &result));
    } else {
      ASSERT_TRUE(memory.ReadCString(strings[0].address(), &result));
      EXPECT_EQ(result, strings[0].expected_value());
      ASSERT_TRUE(memory.ReadCString(strings[1].address(), &result));
      EXPECT_EQ(result, strings[1].expected_value());
      EXPECT_FALSE(memory.ReadCString(strings[2].address(), &result));
      EXPECT_FALSE(memory.ReadCString(strings[3].address(), &result));
    }
  }

  const bool limit_size_;
};

TEST(ProcessMemory, ReadCStringUnmappedChild) {
  ReadCStringUnmappedTest test(/* limit_size= */ false);
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstChild();
}

TEST(ProcessMemory, ReadCStringSizeLimitedUnmappedChild) {
  ReadCStringUnmappedTest test(/* limit_size= */ true);
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstChild();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
