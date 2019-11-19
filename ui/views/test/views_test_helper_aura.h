// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_HELPER_AURA_H_
#define UI_VIEWS_TEST_VIEWS_TEST_HELPER_AURA_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/test/views_test_helper.h"

namespace aura {
namespace client {
class ScreenPositionClient;
}
namespace test {
class AuraTestHelper;
}
}

namespace views {

class ViewsTestHelperAura : public ViewsTestHelper {
 public:
  ViewsTestHelperAura(ui::ContextFactory* context_factory,
                      ui::ContextFactoryPrivate* context_factory_private);
  ~ViewsTestHelperAura() override;

  // Overridden from ViewsTestHelper:
  void SetUp() override;
  void TearDown() override;
  gfx::NativeWindow GetContext() override;

 private:
  ui::ContextFactory* context_factory_;
  ui::ContextFactoryPrivate* context_factory_private_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;

  DISALLOW_COPY_AND_ASSIGN(ViewsTestHelperAura);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_HELPER_AURA_H_
