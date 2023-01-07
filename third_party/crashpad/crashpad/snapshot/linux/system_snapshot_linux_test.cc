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

#include "snapshot/linux/system_snapshot_linux.h"

#include <sys/time.h>
#include <unistd.h>

#include <string>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "snapshot/linux/process_reader_linux.h"
#include "test/errors.h"
#include "test/linux/fake_ptrace_connection.h"

namespace crashpad {
namespace test {
namespace {

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST(SystemSnapshotLinux, MAYBE_Basic) {
  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));

  ProcessReaderLinux process_reader;
  ASSERT_TRUE(process_reader.Initialize(&connection));

  timeval snapshot_time;
  ASSERT_EQ(gettimeofday(&snapshot_time, nullptr), 0)
      << ErrnoMessage("gettimeofday");

  internal::SystemSnapshotLinux system;
  system.Initialize(&process_reader, &snapshot_time);

  EXPECT_GT(system.CPUCount(), 0u);

  uint64_t current_hz, max_hz;
  system.CPUFrequency(&current_hz, &max_hz);
  // For short-term loads, modern CPUs can boost single-core frequency beyond
  // the advertised base clock. Let's assume this is no more than a factor 2.
  EXPECT_GE(max_hz * 2, current_hz);

  int major, minor, bugfix;
  std::string build;
  system.OSVersion(&major, &minor, &bugfix, &build);
  EXPECT_GE(major, 3);
  EXPECT_GE(minor, 0);
  EXPECT_GE(bugfix, 0);
  EXPECT_FALSE(build.empty());

  EXPECT_FALSE(system.OSVersionFull().empty());

  // No expectations; just make sure these can be called successfully.
  system.CPURevision();
  system.NXEnabled();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(system.MachineDescription().empty());
#else
  system.MachineDescription();
#endif  // BUILDFLAG(IS_ANDROID)

#if defined(ARCH_CPU_X86_FAMILY)
  system.CPUX86Signature();
  system.CPUX86Features();
  system.CPUX86ExtendedFeatures();
  system.CPUX86Leaf7Features();

  EXPECT_PRED1(
      [](std::string vendor) {
        return vendor == "GenuineIntel" || vendor == "AuthenticAMD" ||
               vendor == "HygonGenuine";
      },
      system.CPUVendor());

  EXPECT_TRUE(system.CPUX86SupportsDAZ());
#endif  // ARCH_CPU_X86_FAMILY
}

}  // namespace
}  // namespace test
}  // namespace crashpad
