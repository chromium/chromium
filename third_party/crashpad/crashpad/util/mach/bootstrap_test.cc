// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "util/mach/bootstrap.h"

#include "base/mac/scoped_mach_port.h"
#include "gtest/gtest.h"
#include "util/mach/mach_extensions.h"
#include "util/misc/random_string.h"

namespace crashpad {
namespace test {
namespace {

TEST(Bootstrap, BootstrapCheckInAndLookUp) {
  // This should always exist.
  base::mac::ScopedMachSendRight report_crash(
      BootstrapLookUp("com.apple.ReportCrash"));
  EXPECT_NE(report_crash, kMachPortNull);

  std::string service_name = "org.chromium.crashpad.test.bootstrap_check_in.";
  service_name.append(RandomString());

  {
    // The new service hasn’t checked in yet, so this should fail.
    base::mac::ScopedMachSendRight send(BootstrapLookUp(service_name));
    EXPECT_EQ(send, kMachPortNull);

    // Check it in.
    base::mac::ScopedMachReceiveRight receive(BootstrapCheckIn(service_name));
    EXPECT_NE(receive, kMachPortNull);

    // Now it should be possible to look up the new service.
    send = BootstrapLookUp(service_name);
    EXPECT_NE(send, kMachPortNull);

    // It shouldn’t be possible to check the service in while it’s active.
    base::mac::ScopedMachReceiveRight receive_2(BootstrapCheckIn(service_name));
    EXPECT_EQ(receive_2, kMachPortNull);
  }

  // The new service should be gone now.
  base::mac::ScopedMachSendRight send(BootstrapLookUp(service_name));
  EXPECT_EQ(send, kMachPortNull);

  // It should be possible to check it in again.
  base::mac::ScopedMachReceiveRight receive(BootstrapCheckIn(service_name));
  EXPECT_NE(receive, kMachPortNull);
}

TEST(Bootstrap, SystemCrashReporterHandler) {
  base::mac::ScopedMachSendRight system_crash_reporter_handler(
      SystemCrashReporterHandler());
  EXPECT_TRUE(system_crash_reporter_handler.is_valid());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
