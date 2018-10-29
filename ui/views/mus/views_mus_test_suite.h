// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_VIEWS_MUS_TEST_SUITE_H_
#define UI_VIEWS_MUS_VIEWS_MUS_TEST_SUITE_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "ui/views/views_test_suite.h"

namespace mojo {
namespace core {
class ScopedIPCSupport;
}
}  // namespace mojo

namespace views {

class ViewsMusTestSuite : public ViewsTestSuite {
 public:
  ViewsMusTestSuite(int argc, char** argv);
  ~ViewsMusTestSuite() override;

 private:
  // ViewsTestSuite:
  void Initialize() override;
  void InitializeEnv() override;
  void DestroyEnv() override;

  base::Thread ipc_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<aura::Env> env_;

  DISALLOW_COPY_AND_ASSIGN(ViewsMusTestSuite);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_VIEWS_MUS_TEST_SUITE_H_
