// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"

namespace {

class LevelDbTestSuite : public base::TestSuite {
 public:
  LevelDbTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  LevelDbTestSuite(const LevelDbTestSuite&) = delete;
  LevelDbTestSuite& operator=(const LevelDbTestSuite&) = delete;

  ~LevelDbTestSuite() override = default;

  void Initialize() override {
    base::TestSuite::Initialize();
    task_environment_.emplace();
  }

  void Shutdown() override {
    task_environment_.reset();
    base::TestSuite::Shutdown();
  }

 private:
  // Chromium's leveldb::Env uses PostTask.
  std::optional<base::test::TaskEnvironment> task_environment_;
};

}  // namespace

int main(int argc, char** argv) {
  LevelDbTestSuite test_suite(argc, argv);

  // Many tests reuse the same database path and so must run serially.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&LevelDbTestSuite::Run, base::Unretained(&test_suite)));
}
