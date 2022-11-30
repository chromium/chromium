// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_TEST_SUITE_H_
#define UI_VIEWS_VIEWS_TEST_SUITE_H_

#include "base/memory/raw_ptr.h"
#include "base/test/test_suite.h"

#include "build/build_config.h"

#if defined(USE_AURA)
#include <memory>

namespace aura {
class Env;
}
#endif

namespace views {

class ViewsTestSuite : public base::TestSuite {
 public:
  ViewsTestSuite(int argc, char** argv);

  ViewsTestSuite(const ViewsTestSuite&) = delete;
  ViewsTestSuite& operator=(const ViewsTestSuite&) = delete;

  ~ViewsTestSuite() override;

  int RunTests();
  int RunTestsSerially();

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

#if defined(USE_AURA)
  // Different test suites may wish to create Env differently.
  virtual void InitializeEnv();
  virtual void DestroyEnv();
#endif

 private:
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif

  int argc_;
  raw_ptr<char*> argv_;
};

}  // namespace views

#endif  // UI_VIEWS_VIEWS_TEST_SUITE_H_
