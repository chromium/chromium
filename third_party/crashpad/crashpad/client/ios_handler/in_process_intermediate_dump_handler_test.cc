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

#include "client/ios_handler/in_process_intermediate_dump_handler.h"

#include <sys/utsname.h>

#include <iterator>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "client/annotation.h"
#include "client/annotation_list.h"
#include "client/crashpad_info.h"
#include "client/simple_string_dictionary.h"
#include "gtest/gtest.h"
#include "snapshot/ios/process_snapshot_ios_intermediate_dump.h"
#include "test/scoped_set_thread_name.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"
#include "util/file/filesystem.h"
#include "util/misc/capture_context.h"

namespace crashpad {
namespace test {
namespace {

using internal::InProcessIntermediateDumpHandler;

class InProcessIntermediateDumpHandlerTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    writer_ = std::make_unique<internal::IOSIntermediateDumpWriter>();
    EXPECT_TRUE(writer_->Open(path_));
    ASSERT_TRUE(IsRegularFile(path_));
  }

  void TearDown() override {
    EXPECT_TRUE(writer_->Close());
    writer_.reset();
    EXPECT_FALSE(IsRegularFile(path_));
  }

  void WriteReportAndCloseWriter() {
    {
      internal::IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer_.get());
      InProcessIntermediateDumpHandler::WriteHeader(writer_.get());
      InProcessIntermediateDumpHandler::WriteProcessInfo(
          writer_.get(), {{"before_dump", "pre"}});
      InProcessIntermediateDumpHandler::WriteSystemInfo(
          writer_.get(), system_data_, ClockMonotonicNanoseconds());
      InProcessIntermediateDumpHandler::WriteThreadInfo(writer_.get(), 0, 0);
      InProcessIntermediateDumpHandler::WriteModuleInfo(writer_.get());
    }
    EXPECT_TRUE(writer_->Close());
  }

  void WriteMachException() {
    crashpad::NativeCPUContext cpu_context;
    crashpad::CaptureContext(&cpu_context);
    const mach_exception_data_type_t code[2] = {};
    static constexpr int kSimulatedException = -1;
    InProcessIntermediateDumpHandler::WriteExceptionFromMachException(
        writer_.get(),
        MACH_EXCEPTION_CODES,
        mach_thread_self(),
        kSimulatedException,
        code,
        std::size(code),
        MACHINE_THREAD_STATE,
        reinterpret_cast<ConstThreadState>(&cpu_context),
        MACHINE_THREAD_STATE_COUNT);
  }

  const auto& path() const { return path_; }
  auto writer() const { return writer_.get(); }

#if TARGET_OS_SIMULATOR
  // macOS 14.0 is 23A344, macOS 13.6.5 is 22G621, so if the first two
  // characters in the kern.osversion are > 22, this build will reproduce the
  // simulator bug in crbug.com/328282286
  bool IsMacOSVersion143OrGreaterAndiOS16OrLess() {
    if (__builtin_available(iOS 17, *)) {
      return false;
    }
    return std::stoi(system_data_.Build().substr(0, 2)) > 22;
  }
#endif

 private:
  std::unique_ptr<internal::IOSIntermediateDumpWriter> writer_;
  internal::IOSSystemDataCollector system_data_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

TEST_F(InProcessIntermediateDumpHandlerTest, TestSystem) {
  WriteReportAndCloseWriter();
  internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), {}));

  // Snpahot
  const SystemSnapshot* system = process_snapshot.System();
  ASSERT_NE(system, nullptr);
#if defined(ARCH_CPU_X86_64)
  EXPECT_EQ(system->GetCPUArchitecture(), kCPUArchitectureX86_64);
  EXPECT_STREQ(system->CPUVendor().c_str(), "GenuineIntel");
#elif defined(ARCH_CPU_ARM64)
  EXPECT_EQ(system->GetCPUArchitecture(), kCPUArchitectureARM64);
#else
#error Port to your CPU architecture
#endif
#if TARGET_OS_SIMULATOR
  EXPECT_EQ(system->MachineDescription().substr(0, 13),
            std::string("iOS Simulator"));
#elif TARGET_OS_IPHONE
  utsname uts;
  ASSERT_EQ(uname(&uts), 0);
  EXPECT_STREQ(system->MachineDescription().c_str(), uts.machine);
#endif

  EXPECT_EQ(system->GetOperatingSystem(), SystemSnapshot::kOperatingSystemIOS);
}

TEST_F(InProcessIntermediateDumpHandlerTest, TestAnnotations) {
#if TARGET_OS_SIMULATOR
  // This test will fail on older (<iOS17 simulators) when running on macOS 14.3
  // or newer due to a bug in Simulator. crbug.com/328282286
  if (IsMacOSVersion143OrGreaterAndiOS16OrLess()) {
    // For TearDown.
    ASSERT_TRUE(LoggingRemoveFile(path()));
    return;
  }
#endif
  // This is “leaked” to crashpad_info.
  crashpad::SimpleStringDictionary* simple_annotations =
      new crashpad::SimpleStringDictionary();
  simple_annotations->SetKeyValue("#TEST# pad", "break");
  simple_annotations->SetKeyValue("#TEST# key", "value");
  simple_annotations->SetKeyValue("#TEST# pad", "crash");
  simple_annotations->SetKeyValue("#TEST# x", "y");
  simple_annotations->SetKeyValue("#TEST# longer", "shorter");
  simple_annotations->SetKeyValue("#TEST# empty_value", "");

  crashpad::CrashpadInfo* crashpad_info =
      crashpad::CrashpadInfo::GetCrashpadInfo();

  crashpad_info->set_simple_annotations(simple_annotations);

  crashpad::AnnotationList::Register();  // This is “leaked” to crashpad_info.

  static crashpad::StringAnnotation<32> test_annotation_one{"#TEST# one"};
  static crashpad::StringAnnotation<32> test_annotation_two{"#TEST# two"};
  static crashpad::StringAnnotation<32> test_annotation_three{
      "#TEST# same-name"};
  static crashpad::StringAnnotation<32> test_annotation_four{
      "#TEST# same-name"};

  test_annotation_one.Set("moocow");
  test_annotation_two.Set("this will be cleared");
  test_annotation_three.Set("same-name 3");
  test_annotation_four.Set("same-name 4");
  test_annotation_two.Clear();

  WriteReportAndCloseWriter();
  internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(
      path(), {{"after_dump", "post"}}));

  auto process_map = process_snapshot.AnnotationsSimpleMap();
  EXPECT_EQ(process_map.size(), 3u);
  EXPECT_EQ(process_map["before_dump"], "pre");
  EXPECT_EQ(process_map["after_dump"], "post");
  EXPECT_TRUE(process_map.find("crashpad_uptime_ns") != process_map.end());

  std::map<std::string, std::string> all_annotations_simple_map;
  std::vector<AnnotationSnapshot> all_annotations;
  for (const auto* module : process_snapshot.Modules()) {
    std::map<std::string, std::string> module_annotations_simple_map =
        module->AnnotationsSimpleMap();
    all_annotations_simple_map.insert(module_annotations_simple_map.begin(),
                                      module_annotations_simple_map.end());

    std::vector<AnnotationSnapshot> annotations = module->AnnotationObjects();
    all_annotations.insert(
        all_annotations.end(), annotations.begin(), annotations.end());
  }

  EXPECT_EQ(all_annotations_simple_map.size(), 5u);
  EXPECT_EQ(all_annotations_simple_map["#TEST# pad"], "crash");
  EXPECT_EQ(all_annotations_simple_map["#TEST# key"], "value");
  EXPECT_EQ(all_annotations_simple_map["#TEST# x"], "y");
  EXPECT_EQ(all_annotations_simple_map["#TEST# longer"], "shorter");
  EXPECT_EQ(all_annotations_simple_map["#TEST# empty_value"], "");

  bool saw_same_name_3 = false, saw_same_name_4 = false;
  for (const auto& annotation : all_annotations) {
    EXPECT_EQ(annotation.type,
              static_cast<uint16_t>(Annotation::Type::kString));
    std::string value(reinterpret_cast<const char*>(annotation.value.data()),
                      annotation.value.size());
    if (annotation.name == "#TEST# one") {
      EXPECT_EQ(value, "moocow");
    } else if (annotation.name == "#TEST# same-name") {
      if (value == "same-name 3") {
        EXPECT_FALSE(saw_same_name_3);
        saw_same_name_3 = true;
      } else if (value == "same-name 4") {
        EXPECT_FALSE(saw_same_name_4);
        saw_same_name_4 = true;
      } else {
        ADD_FAILURE() << "unexpected annotation value " << value;
      }
    } else {
      ADD_FAILURE() << "unexpected annotation " << annotation.name;
    }
  }
}

TEST_F(InProcessIntermediateDumpHandlerTest, TestThreads) {
  const ScopedSetThreadName scoped_set_thread_name("TestThreads");

  WriteReportAndCloseWriter();
  internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), {}));

  const auto& threads = process_snapshot.Threads();
  ASSERT_GT(threads.size(), 0u);

  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  ASSERT_EQ(thread_info(mach_thread_self(),
                        THREAD_IDENTIFIER_INFO,
                        reinterpret_cast<thread_info_t>(&identifier_info),
                        &count),
            0);
  EXPECT_EQ(threads[0]->ThreadID(), identifier_info.thread_id);
  EXPECT_EQ(threads[0]->ThreadName(), "TestThreads");
}

TEST_F(InProcessIntermediateDumpHandlerTest, TestProcess) {
  WriteReportAndCloseWriter();
  internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), {}));
  EXPECT_EQ(process_snapshot.ProcessID(), getpid());
}

TEST_F(InProcessIntermediateDumpHandlerTest, TestMachException) {
  WriteReportAndCloseWriter();
  internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), {}));
}

TEST_F(InProcessIntermediateDumpHandlerTest, TestSignalException) {
  WriteReportAndCloseWriter();
  internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), {}));
}

TEST_F(InProcessIntermediateDumpHandlerTest, TestNSException) {
  WriteReportAndCloseWriter();
  internal::ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.InitializeWithFilePath(path(), {}));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
