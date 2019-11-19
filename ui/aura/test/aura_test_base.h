// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_BASE_H_
#define UI_AURA_TEST_AURA_TEST_BASE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_helper.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace ui {
class TestContextFactories;
}

namespace aura {
class Window;
class WindowDelegate;

namespace client {
class FocusClient;
}

namespace test {

class AuraTestContextFactory;

// A base class for aura unit tests.
// TODO(beng): Instances of this test will create and own a RootWindow.
class AuraTestBase : public testing::Test {
 public:
  AuraTestBase();
  ~AuraTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Creates a normal window parented to |parent|.
  aura::Window* CreateNormalWindow(int id, Window* parent,
                                   aura::WindowDelegate* delegate);

 protected:
  void RunAllPendingInMessageLoop();

  void ParentWindow(Window* window);

  // A convenience function for dispatching an event to |dispatcher()|.
  // Returns whether |event| was handled.
  bool DispatchEventUsingWindowDispatcher(ui::Event* event);

  Window* root_window() { return helper_->root_window(); }
  WindowTreeHost* host() { return helper_->host(); }
  ui::EventSink* event_sink() { return helper_->event_sink(); }
  TestScreen* test_screen() { return helper_->test_screen(); }
  client::FocusClient* focus_client() { return helper_->focus_client(); }

 private:
  base::test::TaskEnvironment task_environment_;

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer_;
#endif

  bool setup_called_ = false;
  bool teardown_called_ = false;
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<AuraTestHelper> helper_;
  std::unique_ptr<AuraTestContextFactory> mus_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuraTestBase);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_BASE_H_
