// Copyright 2021 The Crashpad Authors
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

#include "snapshot/ios/process_snapshot_ios_intermediate_dump.h"

#include <mach-o/loader.h>

#include <algorithm>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "client/annotation.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"
#include "util/file/string_file.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace test {
namespace {

using Key = internal::IntermediateDumpKey;
using internal::IOSIntermediateDumpWriter;
using internal::ProcessSnapshotIOSIntermediateDump;

class ReadToString : public crashpad::MemorySnapshot::Delegate {
 public:
  std::string result;

  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    result = std::string(reinterpret_cast<const char*>(data), size);
    return true;
  }
};

class ProcessSnapshotIOSIntermediateDumpTest : public testing::Test {
 protected:
  ProcessSnapshotIOSIntermediateDumpTest()
      : long_annotation_name_(Annotation::kNameMaxLength, 'a'),
        long_annotation_value_(Annotation::kValueMaxSize, 'b') {}

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    writer_ = std::make_unique<internal::IOSIntermediateDumpWriter>();
    EXPECT_TRUE(writer_->Open(path_));
    ASSERT_TRUE(IsRegularFile(path_));
  }

  void TearDown() override {
    CloseWriter();
    writer_.reset();
    EXPECT_FALSE(IsRegularFile(path_));
  }

  const auto& path() const { return path_; }
  const auto& annotations() const { return annotations_; }
  auto writer() const { return writer_.get(); }

  bool DumpSnapshot(const ProcessSnapshotIOSIntermediateDump& snapshot) {
    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&snapshot);
    StringFile string_file;
    return minidump.WriteEverything(&string_file);
  }

  void WriteProcessInfo(IOSIntermediateDumpWriter* writer) {
    IOSIntermediateDumpWriter::ScopedMap map(writer, Key::kProcessInfo);
    pid_t pid = 2;
    pid_t parent = 1;
    EXPECT_TRUE(writer->AddProperty(Key::kPID, &pid));
    EXPECT_TRUE(writer->AddProperty(Key::kParentPID, &parent));
    timeval start_time = {12, 0};
    EXPECT_TRUE(writer->AddProperty(Key::kStartTime, &start_time));

    time_value_t user_time = {20, 0};
    time_value_t system_time = {30, 0};
    {
      IOSIntermediateDumpWriter::ScopedMap taskInfo(writer,
                                                    Key::kTaskBasicInfo);
      EXPECT_TRUE(writer->AddProperty(Key::kUserTime, &user_time));
      EXPECT_TRUE(writer->AddProperty(Key::kSystemTime, &system_time));
    }
    {
      IOSIntermediateDumpWriter::ScopedMap taskThreadTimesMap(
          writer, Key::kTaskThreadTimes);
      writer->AddProperty(Key::kUserTime, &user_time);
      writer->AddProperty(Key::kSystemTime, &system_time);
    }

    timeval snapshot_time = {42, 0};
    writer->AddProperty(Key::kSnapshotTime, &snapshot_time);
  }

  void WriteSystemInfo(IOSIntermediateDumpWriter* writer) {
    IOSIntermediateDumpWriter::ScopedMap map(writer, Key::kSystemInfo);
    std::string machine_description = "Gibson";
    EXPECT_TRUE(writer->AddProperty(Key::kMachineDescription,
                                    machine_description.c_str(),
                                    machine_description.length()));
    int os_version_major = 1995;
    int os_version_minor = 9;
    int os_version_bugfix = 15;
    EXPECT_TRUE(writer->AddProperty(Key::kOSVersionMajor, &os_version_major));
    EXPECT_TRUE(writer->AddProperty(Key::kOSVersionMinor, &os_version_minor));
    EXPECT_TRUE(writer->AddProperty(Key::kOSVersionBugfix, &os_version_bugfix));
    std::string os_version_build = "Da Vinci";
    writer->AddProperty(Key::kOSVersionBuild,
                        os_version_build.c_str(),
                        os_version_build.length());

    int cpu_count = 1;
    EXPECT_TRUE(writer->AddProperty(Key::kCpuCount, &cpu_count));
    std::string cpu_vendor = "RISC";
    EXPECT_TRUE(writer->AddProperty(
        Key::kCpuVendor, cpu_vendor.c_str(), cpu_vendor.length()));

    bool has_daylight_saving_time = true;
    EXPECT_TRUE(writer->AddProperty(Key::kHasDaylightSavingTime,
                                    &has_daylight_saving_time));
    bool is_daylight_saving_time = true;
    EXPECT_TRUE(writer->AddProperty(Key::kIsDaylightSavingTime,
                                    &is_daylight_saving_time));
    int standard_offset_seconds = 7200;
    EXPECT_TRUE(writer->AddProperty(Key::kStandardOffsetSeconds,
                                    &standard_offset_seconds));
    int daylight_offset_seconds = 3600;
    EXPECT_TRUE(writer->AddProperty(Key::kDaylightOffsetSeconds,
                                    &daylight_offset_seconds));
    std::string standard_name = "Standard";
    EXPECT_TRUE(writer->AddProperty(
        Key::kStandardName, standard_name.c_str(), standard_name.length()));
    std::string daylight_name = "Daylight";
    EXPECT_TRUE(writer->AddProperty(
        Key::kDaylightName, daylight_name.c_str(), daylight_name.length()));

    vm_size_t page_size = getpagesize();
    EXPECT_TRUE(writer->AddProperty(Key::kPageSize, &page_size));
    {
      natural_t count = 0;
      IOSIntermediateDumpWriter::ScopedMap vmStatMap(writer, Key::kVMStat);
      EXPECT_TRUE(writer->AddProperty(Key::kActive, &count));
      EXPECT_TRUE(writer->AddProperty(Key::kInactive, &count));
      EXPECT_TRUE(writer->AddProperty(Key::kWired, &count));
      EXPECT_TRUE(writer->AddProperty(Key::kFree, &count));
    }

    uint64_t crashpad_report_time_nanos = 1234567890;
    EXPECT_TRUE(
        writer->AddProperty(Key::kCrashpadUptime, &crashpad_report_time_nanos));
  }

  void WriteAnnotations(IOSIntermediateDumpWriter* writer,
                        bool use_long_annotations) {
    constexpr char short_annotation_name[] = "annotation_name";
    constexpr char short_annotation_value[] = "annotation_value";
    const char* const annotation_name = use_long_annotations
                                            ? long_annotation_name_.c_str()
                                            : short_annotation_name;
    const char* const annotation_value = use_long_annotations
                                             ? long_annotation_value_.c_str()
                                             : short_annotation_value;
    {
      IOSIntermediateDumpWriter::ScopedArray annotationObjectArray(
          writer, Key::kAnnotationObjects);
      {
        IOSIntermediateDumpWriter::ScopedArrayMap annotationMap(writer);
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationName, annotation_name, strlen(annotation_name)));
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationValue, annotation_value, strlen(annotation_value)));
        Annotation::Type type = Annotation::Type::kString;
        EXPECT_TRUE(writer->AddProperty(Key::kAnnotationType, &type));
      }
    }
    {
      IOSIntermediateDumpWriter::ScopedArray annotationsSimpleArray(
          writer, Key::kAnnotationsSimpleMap);
      {
        IOSIntermediateDumpWriter::ScopedArrayMap annotationMap(writer);
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationName, annotation_name, strlen(annotation_name)));
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationValue, annotation_value, strlen(annotation_value)));
      }
    }

    IOSIntermediateDumpWriter::ScopedMap annotationMap(
        writer, Key::kAnnotationsCrashInfo);
    {
      EXPECT_TRUE(writer->AddPropertyBytes(Key::kAnnotationsCrashInfoMessage1,
                                           annotation_value,
                                           strlen(annotation_value)));
      EXPECT_TRUE(writer->AddPropertyBytes(Key::kAnnotationsCrashInfoMessage2,
                                           annotation_value,
                                           strlen(annotation_value)));
    }
  }

  void WriteModules(IOSIntermediateDumpWriter* writer,
                    bool has_module_path,
                    bool use_long_annotations) {
    IOSIntermediateDumpWriter::ScopedArray moduleArray(writer, Key::kModules);
    for (uint32_t image_index = 0; image_index < 2; ++image_index) {
      IOSIntermediateDumpWriter::ScopedArrayMap modules(writer);

      if (has_module_path) {
        constexpr char image_file[] = "/path/to/module";
        EXPECT_TRUE(
            writer->AddProperty(Key::kName, image_file, strlen(image_file)));
      }

      uint64_t address = 0;
      uint64_t vmsize = 1;
      uintptr_t imageFileModDate = 2;
      uint32_t current_version = 3;
      uint32_t filetype = MH_DYLIB;
      uint64_t source_version = 5;
      static constexpr uint8_t uuid[16] = {0x00,
                                           0x01,
                                           0x02,
                                           0x03,
                                           0x04,
                                           0x05,
                                           0x06,
                                           0x07,
                                           0x08,
                                           0x09,
                                           0x0a,
                                           0x0b,
                                           0x0c,
                                           0x0d,
                                           0x0e,
                                           0x0f};
      EXPECT_TRUE(writer->AddProperty(Key::kAddress, &address));
      EXPECT_TRUE(writer->AddProperty(Key::kSize, &vmsize));
      EXPECT_TRUE(writer->AddProperty(Key::kTimestamp, &imageFileModDate));
      EXPECT_TRUE(
          writer->AddProperty(Key::kDylibCurrentVersion, &current_version));
      EXPECT_TRUE(writer->AddProperty(Key::kSourceVersion, &source_version));
      EXPECT_TRUE(writer->AddProperty(Key::kUUID, &uuid));
      EXPECT_TRUE(writer->AddProperty(Key::kFileType, &filetype));
      WriteAnnotations(writer, use_long_annotations);
    }
  }

  void ExpectModules(const std::vector<const ModuleSnapshot*>& modules,
                     bool expect_module_path,
                     bool expect_long_annotations) {
    for (auto module : modules) {
      EXPECT_EQ(module->GetModuleType(),
                ModuleSnapshot::kModuleTypeSharedLibrary);

      if (expect_module_path) {
        EXPECT_STREQ(module->Name().c_str(), "/path/to/module");
        EXPECT_STREQ(module->DebugFileName().c_str(), "module");
      }
      UUID uuid;
      uint32_t age;
      module->UUIDAndAge(&uuid, &age);
      EXPECT_EQ(uuid.ToString(), "00010203-0405-0607-0809-0a0b0c0d0e0f");

      for (auto annotation : module->AnnotationsVector()) {
        if (expect_long_annotations) {
          EXPECT_EQ(annotation, long_annotation_value_);
        } else {
          EXPECT_STREQ(annotation.c_str(), "annotation_value");
        }
      }

      for (const auto& it : module->AnnotationsSimpleMap()) {
        if (expect_long_annotations) {
          EXPECT_EQ(it.first, long_annotation_name_);
          EXPECT_EQ(it.second, long_annotation_value_);
        } else {
          EXPECT_STREQ(it.first.c_str(), "annotation_name");
          EXPECT_STREQ(it.second.c_str(), "annotation_value");
        }
      }

      for (auto annotation_object : module->AnnotationObjects()) {
        EXPECT_EQ(annotation_object.type, (short)Annotation::Type::kString);
        if (expect_long_annotations) {
          EXPECT_EQ(annotation_object.name, long_annotation_name_);
          EXPECT_EQ(std::string(reinterpret_cast<const char*>(
                                    annotation_object.value.data()),
                                annotation_object.value.size()),
                    long_annotation_value_);
        } else {
          EXPECT_STREQ(annotation_object.name.c_str(), "annotation_name");
          EXPECT_STREQ(std::string(reinterpret_cast<const char*>(
                                       annotation_object.value.data()),
                                   annotation_object.value.size())
                           .c_str(),
                       "annotation_value");
        }
      }
    }
  }

  void WriteMachException(IOSIntermediateDumpWriter* writer,
                          bool short_context = false) {
    IOSIntermediateDumpWriter::ScopedMap machExceptionMap(writer,
                                                          Key::kMachException);
    exception_type_t exception = 5;
    mach_exception_data_type_t code[] = {4, 3};
    mach_msg_type_number_t code_count = 2;

#if defined(ARCH_CPU_X86_64)
    thread_state_flavor_t flavor = x86_THREAD_STATE;
    x86_thread_state_t state = {};
    state.tsh.flavor = x86_THREAD_STATE64;
    state.tsh.count = x86_THREAD_STATE64_COUNT;
    state.uts.ts64.__rip = 0xdeadbeef;
    size_t state_length = sizeof(x86_thread_state_t);
#elif defined(ARCH_CPU_ARM64)
    thread_state_flavor_t flavor = ARM_UNIFIED_THREAD_STATE;
    arm_unified_thread_state_t state = {};
    state.ash.flavor = ARM_THREAD_STATE64;
    state.ash.count = ARM_THREAD_STATE64_COUNT;
    state.ts_64.__pc = 0xdeadbeef;
    size_t state_length = sizeof(arm_unified_thread_state_t);
#endif
    EXPECT_TRUE(writer->AddProperty(Key::kException, &exception));
    EXPECT_TRUE(writer->AddProperty(Key::kCodes, code, code_count));
    EXPECT_TRUE(writer->AddProperty(Key::kFlavor, &flavor));

    if (short_context) {
      state_length -= 10;
    }
    EXPECT_TRUE(writer->AddPropertyBytes(
        Key::kState, reinterpret_cast<const void*>(&state), state_length));
    uint64_t thread_id = 1;
    EXPECT_TRUE(writer->AddProperty(Key::kThreadID, &thread_id));
  }

  void WriteThreads(IOSIntermediateDumpWriter* writer) {
    vm_address_t stack_region_address = 0;
    IOSIntermediateDumpWriter::ScopedArray threadArray(writer, Key::kThreads);
    for (uint64_t thread_id = 1; thread_id < 3; thread_id++) {
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer);
      EXPECT_TRUE(writer->AddProperty(Key::kThreadID, &thread_id));

      integer_t suspend_count = 666;
      integer_t importance = 5;
      uint64_t thread_handle = thread_id;
      EXPECT_TRUE(writer->AddProperty(Key::kSuspendCount, &suspend_count));
      EXPECT_TRUE(writer->AddProperty(Key::kPriority, &importance));
      EXPECT_TRUE(writer->AddProperty(Key::kThreadDataAddress, &thread_handle));

#if defined(ARCH_CPU_X86_64)
      x86_thread_state64_t thread_state = {};
      thread_state.__rip = 0xdeadbeef;
      x86_float_state64_t float_state = {};
      x86_debug_state64_t debug_state = {};
#elif defined(ARCH_CPU_ARM64)
      arm_thread_state64_t thread_state = {};
      thread_state.__pc = 0xdeadbeef;
      arm_neon_state64_t float_state = {};
      arm_debug_state64_t debug_state = {};
#endif
      EXPECT_TRUE(writer->AddProperty(Key::kThreadState, &thread_state));
      EXPECT_TRUE(writer->AddProperty(Key::kFloatState, &float_state));
      EXPECT_TRUE(writer->AddProperty(Key::kDebugState, &debug_state));

      // Non-overlapping stack_region_address.
      stack_region_address += 10;
      EXPECT_TRUE(
          writer->AddProperty(Key::kStackRegionAddress, &stack_region_address));
      EXPECT_TRUE(
          writer->AddPropertyBytes(Key::kStackRegionData, "stack_data", 10));
      {
        IOSIntermediateDumpWriter::ScopedArray memoryRegions(
            writer, Key::kThreadContextMemoryRegions);
        {
          IOSIntermediateDumpWriter::ScopedArrayMap memoryRegion(writer);
          const vm_address_t memory_region_address = 0;
          EXPECT_TRUE(writer->AddProperty(
              Key::kThreadContextMemoryRegionAddress, &memory_region_address));
          EXPECT_TRUE(writer->AddPropertyBytes(
              Key::kThreadContextMemoryRegionData, "string", 6));
        }
      }
      EXPECT_TRUE(writer->AddPropertyBytes(Key::kThreadName, "ariadne", 7));
    }
  }

  void ExpectMachException(const ExceptionSnapshot& exception) {
    EXPECT_EQ(exception.ThreadID(), 1u);
    EXPECT_EQ(exception.Exception(), 5u);
    EXPECT_TRUE(exception.Context()->Is64Bit());
    EXPECT_EQ(exception.Context()->InstructionPointer(), 0xdeadbeef);
    EXPECT_EQ(exception.ExceptionInfo(), 4u);
    EXPECT_EQ(exception.ExceptionAddress(), 0xdeadbeef);
    EXPECT_EQ(exception.Codes()[0], 5u);
    EXPECT_EQ(exception.Codes()[1], 4u);
    EXPECT_EQ(exception.Codes()[2], 3u);
  }

  void ExpectThreads(const std::vector<const ThreadSnapshot*>& threads) {
    uint64_t thread_id = 1;
    for (auto thread : threads) {
      EXPECT_EQ(thread->ThreadID(), thread_id);
      EXPECT_EQ(thread->ThreadName(), "ariadne");
      EXPECT_EQ(thread->SuspendCount(), 666);
      EXPECT_EQ(thread->Priority(), 5);
      EXPECT_EQ(thread->ThreadSpecificDataAddress(), thread_id++);
      ReadToString delegate;
      for (auto memory : thread->ExtraMemory()) {
        memory->Read(&delegate);
        EXPECT_STREQ(delegate.result.c_str(), "string");
      }

      thread->Stack()->Read(&delegate);
      EXPECT_STREQ(delegate.result.c_str(), "stack_data");

      EXPECT_TRUE(thread->Context()->Is64Bit());
      EXPECT_EQ(thread->Context()->InstructionPointer(), 0xdeadbeef);
    }
  }

  void ExpectSystem(const SystemSnapshot& system) {
    EXPECT_EQ(system.CPUCount(), 1u);
    EXPECT_STREQ(system.CPUVendor().c_str(), "RISC");
    int major;
    int minor;
    int bugfix;
    std::string build;
    system.OSVersion(&major, &minor, &bugfix, &build);
    EXPECT_EQ(major, 1995);
    EXPECT_EQ(minor, 9);
    EXPECT_EQ(bugfix, 15);
    EXPECT_STREQ(build.c_str(), "Da Vinci");
    EXPECT_STREQ(system.OSVersionFull().c_str(), "1995.9.15 Da Vinci");
    EXPECT_STREQ(system.MachineDescription().c_str(), "Gibson");

    SystemSnapshot::DaylightSavingTimeStatus dst_status;
    int standard_offset_seconds;
    int daylight_offset_seconds;
    std::string standard_name;
    std::string daylight_name;

    system.TimeZone(&dst_status,
                    &standard_offset_seconds,
                    &daylight_offset_seconds,
                    &standard_name,
                    &daylight_name);
    EXPECT_EQ(standard_offset_seconds, 7200);
    EXPECT_EQ(daylight_offset_seconds, 3600);
    EXPECT_STREQ(standard_name.c_str(), "Standard");
    EXPECT_STREQ(daylight_name.c_str(), "Daylight");
  }

  void ExpectSnapshot(const ProcessSnapshot& snapshot,
                      bool expect_module_path,
                      bool expect_long_annotations) {
    EXPECT_EQ(snapshot.ProcessID(), 2);
    EXPECT_EQ(snapshot.ParentProcessID(), 1);

    timeval snapshot_time;
    snapshot.SnapshotTime(&snapshot_time);
    EXPECT_EQ(snapshot_time.tv_sec, 42);
    EXPECT_EQ(snapshot_time.tv_usec, 0);

    timeval start_time;
    snapshot.ProcessStartTime(&start_time);
    EXPECT_EQ(start_time.tv_sec, 12);
    EXPECT_EQ(start_time.tv_usec, 0);

    timeval user_time, system_time;
    snapshot.ProcessCPUTimes(&user_time, &system_time);
    EXPECT_EQ(user_time.tv_sec, 40);
    EXPECT_EQ(user_time.tv_usec, 0);
    EXPECT_EQ(system_time.tv_sec, 60);
    EXPECT_EQ(system_time.tv_usec, 0);

    ExpectSystem(*snapshot.System());
    ExpectThreads(snapshot.Threads());
    ExpectModules(
        snapshot.Modules(), expect_module_path, expect_long_annotations);
    ExpectMachException(*snapshot.Exception());

    auto map = snapshot.AnnotationsSimpleMap();
    EXPECT_EQ(map["crashpad_uptime_ns"], "1234567890");
  }

  void CloseWriter() { EXPECT_TRUE(writer_->Close()); }

 private:
  std::unique_ptr<internal::IOSIntermediateDumpWriter> writer_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
  std::map<std::string, std::string> annotations_;
  const std::string long_annotation_name_;
  const std::string long_annotation_value_;
};

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, InitializeNoFile) {
  const base::FilePath file;
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  EXPECT_FALSE(process_snapshot.InitializeWithFilePath(file, annotations()));
  EXPECT_TRUE(LoggingRemoveFile(path()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, InitializeEmpty) {
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  EXPECT_FALSE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, InitializeMinimumDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSystemInfo); }
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kProcessInfo); }
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, MissingSystemDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kProcessInfo); }
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_FALSE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, MissingProcessDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSystemInfo); }
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_FALSE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptySignalDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSignalException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
      {
        IOSIntermediateDumpWriter::ScopedArray contextMemoryRegions(
            writer(), Key::kThreadContextMemoryRegions);
        IOSIntermediateDumpWriter::ScopedArrayMap memoryMap(writer());

        std::string random_data("random_data");
        EXPECT_TRUE(writer()->AddProperty(
            Key::kThreadContextMemoryRegionAddress, &thread_id));
        EXPECT_TRUE(writer()->AddProperty(Key::kThreadContextMemoryRegionData,
                                          random_data.c_str(),
                                          random_data.length()));
      }
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
    }
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_EQ(process_snapshot.Exception()->ExtraMemory().size(), 1u);
  ReadToString delegate;
  for (auto memory : process_snapshot.Exception()->ExtraMemory()) {
    memory->Read(&delegate);
    EXPECT_STREQ(delegate.result.c_str(), "random_data");
  }
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyMachDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kMachException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
    }
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyExceptionDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kNSException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
    }
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyUncaughtNSExceptionDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kNSException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
      const uint64_t frames[] = {0, 0};
      const size_t num_frames = 2;
      writer()->AddProperty(
          Key::kThreadUncaughtNSExceptionFrames, frames, num_frames);
    }
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, ShortContext) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    WriteThreads(writer());
    WriteModules(
        writer(), /*has_module_path=*/false, /*use_long_annotations=*/false);
    WriteMachException(writer(), true /* short_context=true*/);
  }
  CloseWriter();

  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
  ExpectSnapshot(process_snapshot,
                 /*expect_module_path=*/false,
                 /*expect_long_annotations=*/false);
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, LongAnnotations) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    WriteThreads(writer());
    WriteModules(
        writer(), /*has_module_path=*/false, /*use_long_annotations=*/true);
    WriteMachException(writer());
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
  ExpectSnapshot(process_snapshot,
                 /*expect_module_path=*/false,
                 /*expect_long_annotations=*/true);
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, FullReport) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    WriteThreads(writer());
    WriteModules(
        writer(), /*has_module_path=*/true, /*use_long_annotations=*/false);
    WriteMachException(writer());
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
  ExpectSnapshot(process_snapshot,
                 /*expect_module_path=*/true,
                 /*expect_long_annotations=*/false);
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, FuzzTestCases) {
  base::FilePath fuzz_path = TestPaths::TestDataRoot().Append(FILE_PATH_LITERAL(
      "snapshot/ios/testdata/crash-1fa088dda0adb41459d063078a0f384a0bb8eefa"));
  crashpad::internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  EXPECT_TRUE(process_snapshot.InitializeWithFilePath(fuzz_path, {}));
  EXPECT_TRUE(LoggingRemoveFile(path()));

  auto map = process_snapshot.AnnotationsSimpleMap();
  ASSERT_TRUE(map.find("crashpad_intermediate_dump_incomplete") != map.end());
  EXPECT_EQ(map["crashpad_intermediate_dump_incomplete"], "yes");

  fuzz_path = TestPaths::TestDataRoot().Append(
      FILE_PATH_LITERAL("snapshot/ios/testdata/crash-5726011582644224"));
  crashpad::internal::ProcessSnapshotIOSIntermediateDump process_snapshot2;
  EXPECT_TRUE(process_snapshot2.InitializeWithFilePath(fuzz_path, {}));
  map = process_snapshot2.AnnotationsSimpleMap();
  ASSERT_TRUE(map.find("crashpad_intermediate_dump_incomplete") != map.end());
  EXPECT_EQ(map["crashpad_intermediate_dump_incomplete"], "yes");

  fuzz_path = TestPaths::TestDataRoot().Append(
      FILE_PATH_LITERAL("snapshot/ios/testdata/crash-6605504629637120"));
  crashpad::internal::ProcessSnapshotIOSIntermediateDump process_snapshot3;
  EXPECT_FALSE(process_snapshot3.InitializeWithFilePath(fuzz_path, {}));

  fuzz_path = TestPaths::TestDataRoot().Append(
      FILE_PATH_LITERAL("snapshot/ios/testdata/crash-c44acfcbccd8c7a8"));
  crashpad::internal::ProcessSnapshotIOSIntermediateDump process_snapshot4;
  EXPECT_TRUE(process_snapshot4.InitializeWithFilePath(fuzz_path, {}));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, WriteNoThreads) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    WriteMachException(writer());
  }
  CloseWriter();
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
