// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/posix/process_info.h"

#include <sys/utsname.h>
#include <time.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/main_arguments.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/misc/implicit_cast.h"
#include "util/string/split_string.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include "util/linux/direct_ptrace_connection.h"
#include "test/linux/fake_ptrace_connection.h"
#endif

namespace crashpad {
namespace test {
namespace {

void TestProcessSelfOrClone(const ProcessInfo& process_info) {
  // There’s no system call to obtain the saved set-user ID or saved set-group
  // ID in an easy way. Normally, they are the same as the effective user ID and
  // effective group ID, so just check against those.
  EXPECT_EQ(process_info.RealUserID(), getuid());
  const uid_t euid = geteuid();
  EXPECT_EQ(process_info.EffectiveUserID(), euid);
  EXPECT_EQ(process_info.SavedUserID(), euid);

  const gid_t gid = getgid();
  EXPECT_EQ(process_info.RealGroupID(), gid);
  const gid_t egid = getegid();
  EXPECT_EQ(process_info.EffectiveGroupID(), egid);
  EXPECT_EQ(process_info.SavedGroupID(), egid);

  // Test SupplementaryGroups().
  int group_count = getgroups(0, nullptr);
  ASSERT_GE(group_count, 0) << ErrnoMessage("getgroups");

  std::vector<gid_t> group_vector(group_count);
  if (group_count > 0) {
    group_count = getgroups(group_vector.size(), &group_vector[0]);
    ASSERT_GE(group_count, 0) << ErrnoMessage("getgroups");
    ASSERT_EQ(implicit_cast<size_t>(group_count), group_vector.size());
  }

  std::set<gid_t> group_set(group_vector.begin(), group_vector.end());
  EXPECT_EQ(process_info.SupplementaryGroups(), group_set);

  // Test AllGroups(), which is SupplementaryGroups() plus the real, effective,
  // and saved set-group IDs. The effective and saved set-group IDs are expected
  // to be identical (see above).
  group_set.insert(gid);
  group_set.insert(egid);

  EXPECT_EQ(process_info.AllGroups(), group_set);

  // The test executable isn’t expected to change privileges.
  EXPECT_FALSE(process_info.DidChangePrivileges());

#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(process_info.Is64Bit());
#else
  EXPECT_FALSE(process_info.Is64Bit());
#endif

  // Test StartTime(). This program must have started at some time in the past.
  timeval start_time;
  ASSERT_TRUE(process_info.StartTime(&start_time));
  EXPECT_FALSE(start_time.tv_sec == 0 && start_time.tv_usec == 0);
  time_t now;
  time(&now);
  EXPECT_LE(start_time.tv_sec, now);

  const std::vector<std::string>& expect_argv = GetMainArguments();

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Prior to Linux 4.2, the kernel only allowed reading a single page from
  // /proc/<pid>/cmdline, causing any further arguments to be truncated. Disable
  // testing arguments in this case.
  // TODO(jperaza): The main arguments are stored on the main thread's stack
  // (and so should be included in dumps automatically), and
  // ProcessInfo::Arguments() might be updated to read the arguments directly,
  // rather than via procfs on older kernels.
  utsname uts;
  ASSERT_EQ(uname(&uts), 0) << ErrnoMessage("uname");
  std::vector<std::string> parts = SplitString(uts.release, '.');
  ASSERT_GE(parts.size(), 2u);

  int major, minor;
  ASSERT_TRUE(base::StringToInt(parts[0], &major));
  ASSERT_TRUE(base::StringToInt(parts[1], &minor));

  size_t argv_size = 0;
  for (const auto& arg : expect_argv) {
    argv_size += arg.size() + 1;
  }

  if ((major < 4 || (major == 4 && minor < 2)) &&
      argv_size > static_cast<size_t>(getpagesize())) {
    return;
  }
#endif  // OS_ANDROID || OS_LINUX || OS_CHROMEOS

  std::vector<std::string> argv;
  ASSERT_TRUE(process_info.Arguments(&argv));

  // expect_argv always contains the initial view of the arguments at the time
  // the program was invoked. argv may contain this view, or it may contain the
  // current view of arguments after Google Test argv processing. argv may be a
  // subset of expect_argv.
  //
  // Google Test argv processing always leaves argv[0] intact, so this can be
  // checked directly.
  ASSERT_FALSE(expect_argv.empty());
  ASSERT_FALSE(argv.empty());
  EXPECT_EQ(argv[0], expect_argv[0]);

  EXPECT_LE(argv.size(), expect_argv.size());

  // Everything else in argv should have a match in expect_argv too, but things
  // may have moved around.
  for (size_t arg_index = 1; arg_index < argv.size(); ++arg_index) {
    const std::string& arg = argv[arg_index];
    SCOPED_TRACE(
        base::StringPrintf("arg_index %zu, arg %s", arg_index, arg.c_str()));
    EXPECT_NE(expect_argv.end(), std::find(argv.begin(), argv.end(), arg));
  }
}

void TestSelfProcess(const ProcessInfo& process_info) {
  EXPECT_EQ(process_info.ProcessID(), getpid());
  EXPECT_EQ(process_info.ParentProcessID(), getppid());

  TestProcessSelfOrClone(process_info);
}

TEST(ProcessInfo, Self) {
  ProcessInfo process_info;
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));
  ASSERT_TRUE(process_info.InitializeWithPtrace(&connection));
#else
  ASSERT_TRUE(process_info.InitializeWithPid(getpid()));
#endif  // OS_LINUX || OS_ANDROID || OS_CHROMEOS

  TestSelfProcess(process_info);
}

#if defined(OS_APPLE)
TEST(ProcessInfo, SelfTask) {
  ProcessInfo process_info;
  ASSERT_TRUE(process_info.InitializeWithTask(mach_task_self()));
  TestSelfProcess(process_info);
}
#endif

TEST(ProcessInfo, Pid1) {
  // PID 1 is expected to be init or the system’s equivalent. This tests reading
  // information about another process.
  ProcessInfo process_info;
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(1));
  ASSERT_TRUE(process_info.InitializeWithPtrace(&connection));
#else
  ASSERT_TRUE(process_info.InitializeWithPid(1));
#endif

  EXPECT_EQ(process_info.ProcessID(), implicit_cast<pid_t>(1));
  EXPECT_EQ(process_info.ParentProcessID(), implicit_cast<pid_t>(0));
  EXPECT_EQ(process_info.RealUserID(), implicit_cast<uid_t>(0));
  EXPECT_EQ(process_info.EffectiveUserID(), implicit_cast<uid_t>(0));
  EXPECT_EQ(process_info.SavedUserID(), implicit_cast<uid_t>(0));
  EXPECT_EQ(process_info.RealGroupID(), implicit_cast<gid_t>(0));
  EXPECT_EQ(process_info.EffectiveGroupID(), implicit_cast<gid_t>(0));
  EXPECT_EQ(process_info.SavedGroupID(), implicit_cast<gid_t>(0));
  EXPECT_FALSE(process_info.AllGroups().empty());
}

class ProcessInfoForkedTest : public Multiprocess {
 public:
  ProcessInfoForkedTest() : Multiprocess() {}
  ~ProcessInfoForkedTest() {}

  // Multiprocess:
  void MultiprocessParent() override {
    const pid_t pid = ChildPID();

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
    DirectPtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(pid));

    ProcessInfo process_info;
    ASSERT_TRUE(process_info.InitializeWithPtrace(&connection));
#else
    ProcessInfo process_info;
    ASSERT_TRUE(process_info.InitializeWithPid(pid));
#endif  // OS_LINUX || OS_CHROMEOS || OS_ANDROID

    EXPECT_EQ(process_info.ProcessID(), pid);
    EXPECT_EQ(process_info.ParentProcessID(), getpid());

    TestProcessSelfOrClone(process_info);
  }

  void MultiprocessChild() override {
    // Hang around until the parent is done.
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessInfoForkedTest);
};

TEST(ProcessInfo, Forked) {
  ProcessInfoForkedTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
