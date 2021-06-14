// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_HELPER_AURA_H_
#define UI_VIEWS_TEST_VIEWS_TEST_HELPER_AURA_H_

#include <memory>

#include "ui/aura/test/aura_test_helper.h"
#include "ui/views/test/views_test_helper.h"

namespace views {

class ViewsTestHelperAura : public ViewsTestHelper {
 public:
  using AuraTestHelperFactory =
      std::unique_ptr<aura::test::AuraTestHelper> (*)();
  using TestViewsDelegateFactory = std::unique_ptr<TestViewsDelegate> (*)();

  ViewsTestHelperAura();
  ViewsTestHelperAura(const ViewsTestHelperAura&) = delete;
  ViewsTestHelperAura& operator=(const ViewsTestHelperAura&) = delete;
  ~ViewsTestHelperAura() override;

  // ViewsTestHelper:
  std::unique_ptr<TestViewsDelegate> GetFallbackTestViewsDelegate() override;
  void SetUp() override;
  gfx::NativeWindow GetContext() override;

  // Provides a way for test bases to customize what test helper will be used
  // for |aura_test_helper_|.
  static void SetAuraTestHelperFactory(AuraTestHelperFactory factory);

  // Provides a way for test helpers to customize what delegate will be used
  // if one is not provided by the test/framework.
  static void SetFallbackTestViewsDelegateFactory(
      TestViewsDelegateFactory factory);

 private:
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_HELPER_AURA_H_
