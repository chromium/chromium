// Copyright 2015 The Crashpad Authors
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

#include "test/win/win_multiprocess.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

template <int ExitCode>
class TestWinMultiprocess final : public WinMultiprocess {
 public:
  TestWinMultiprocess() {}

  TestWinMultiprocess(const TestWinMultiprocess&) = delete;
  TestWinMultiprocess& operator=(const TestWinMultiprocess&) = delete;

 private:
  // WinMultiprocess will have already exercised the pipes.
  void WinMultiprocessParent() override { SetExpectedChildExitCode(ExitCode); }

  void WinMultiprocessChild() override {
    exit(ExitCode);
  }
};

class TestWinMultiprocessChildAsserts final : public WinMultiprocess {
 public:
  TestWinMultiprocessChildAsserts() {}

  TestWinMultiprocessChildAsserts(const TestWinMultiprocessChildAsserts&) =
      delete;
  TestWinMultiprocessChildAsserts& operator=(
      const TestWinMultiprocessChildAsserts&) = delete;

 private:
  void WinMultiprocessParent() override { SetExpectedChildExitCode(255); }
  void WinMultiprocessChild() override {
    ASSERT_FALSE(true);
  }
};

class TestWinMultiprocessChildExpects final : public WinMultiprocess {
 public:
  TestWinMultiprocessChildExpects() {}

  TestWinMultiprocessChildExpects(const TestWinMultiprocessChildExpects&) =
      delete;
  TestWinMultiprocessChildExpects& operator=(
      const TestWinMultiprocessChildExpects&) = delete;

 private:
  void WinMultiprocessParent() override { SetExpectedChildExitCode(255); }
  void WinMultiprocessChild() override {
    EXPECT_FALSE(true);
  }
};

TEST(WinMultiprocess, WinMultiprocess) {
  WinMultiprocess::Run<TestWinMultiprocess<0>>();
}

TEST(WinMultiprocess, WinMultiprocessNonSuccessExitCode) {
  WinMultiprocess::Run<TestWinMultiprocess<100>>();
}

TEST(WinMultiprocessChildFails, ChildExpectFailure) {
  WinMultiprocess::Run<TestWinMultiprocessChildExpects>();
}

TEST(WinMultiprocessChildFails, ChildAssertFailure) {
  WinMultiprocess::Run<TestWinMultiprocessChildAsserts>();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
