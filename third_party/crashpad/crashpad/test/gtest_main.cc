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

#include "base/logging.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/main_arguments.h"
#include "test/multiprocess_exec.h"

#if defined(CRASHPAD_TEST_LAUNCHER_GOOGLEMOCK)
#include "gmock/gmock.h"
#endif  // CRASHPAD_TEST_LAUNCHER_GOOGLEMOCK

#if defined(OS_ANDROID)
#include "util/linux/initial_signal_dispositions.h"
#endif  // OS_ANDROID

#if defined(OS_IOS)
#include "test/ios/google_test_setup.h"
#endif

#if defined(OS_WIN)
#include "test/win/win_child_process.h"
#endif  // OS_WIN

#if defined(CRASHPAD_IS_IN_CHROMIUM)
#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#endif  // CRASHPAD_IS_IN_CHROMIUM

namespace {

#if !defined(OS_IOS)
bool GetChildTestFunctionName(std::string* child_func_name) {
  constexpr size_t arg_length =
      sizeof(crashpad::test::internal::kChildTestFunction) - 1;
  for (const auto& it : crashpad::test::GetMainArguments()) {
    if (it.compare(
            0, arg_length, crashpad::test::internal::kChildTestFunction) == 0) {
      *child_func_name = it.substr(arg_length);
      return true;
    }
  }
  return false;
}
#endif  // !OS_IOS

}  // namespace

int main(int argc, char* argv[]) {
#if defined(OS_ANDROID)
  crashpad::InitializeSignalDispositions();
#endif  // OS_ANDROID

  crashpad::test::InitializeMainArguments(argc, argv);

#if !defined(OS_IOS)
  std::string child_func_name;
  if (GetChildTestFunctionName(&child_func_name)) {
    return crashpad::test::internal::CheckedInvokeMultiprocessChild(
        child_func_name);
  }
#endif  // !OS_IOS

#if defined(CRASHPAD_IS_IN_CHROMIUM)

#if defined(OS_WIN)
  // Chromium’s test launcher interferes with WinMultiprocess-based tests. Allow
  // their child processes to be launched by the standard Google Test-based test
  // runner.
  const bool use_chromium_test_launcher =
      !crashpad::test::WinChildProcess::IsChildProcess();
#elif defined(OS_ANDROID)
  constexpr bool use_chromium_test_launcher = false;
#else  // OS_WIN
  constexpr bool use_chromium_test_launcher = true;
#endif  // OS_WIN

  if (use_chromium_test_launcher) {
    // This supports --test-launcher-summary-output, which writes a JSON file
    // containing test details needed by Swarming.
    base::TestSuite test_suite(argc, argv);
    return base::LaunchUnitTests(
        argc,
        argv,
        base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
  }

#endif  // CRASHPAD_IS_IN_CHROMIUM

  // base::TestSuite initializes logging when using Chromium's test launcher.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_STDERR | logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

#if defined(CRASHPAD_TEST_LAUNCHER_GOOGLEMOCK)
  testing::InitGoogleMock(&argc, argv);
#elif defined(CRASHPAD_TEST_LAUNCHER_GOOGLETEST)
  testing::InitGoogleTest(&argc, argv);
#else  // CRASHPAD_TEST_LAUNCHER_GOOGLEMOCK
#error #define CRASHPAD_TEST_LAUNCHER_GOOGLETEST or \
    CRASHPAD_TEST_LAUNCHER_GOOGLEMOCK
#endif  // CRASHPAD_TEST_LAUNCHER_GOOGLEMOCK

#if defined(OS_IOS)
  // iOS needs to run tests within the context of an app, so call a helper that
  // invokes UIApplicationMain().  The application delegate will call
  // RUN_ALL_TESTS() and exit before returning control to this function.
  crashpad::test::IOSLaunchApplicationAndRunTests(argc, argv);
#else
  return RUN_ALL_TESTS();
#endif
}
