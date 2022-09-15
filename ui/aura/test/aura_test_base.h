// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_BASE_H_
#define UI_AURA_TEST_AURA_TEST_BASE_H_

#include <memory>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window_tree_host.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace aura {
class Window;
class WindowDelegate;

namespace client {
class FocusClient;
}

namespace test {

// A base class for aura unit tests.
// TODO(beng): Instances of this test will create and own a RootWindow.
class AuraTestBase : public testing::Test {
 public:
  AuraTestBase();

  AuraTestBase(const AuraTestBase&) = delete;
  AuraTestBase& operator=(const AuraTestBase&) = delete;

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

  Window* root_window() { return helper_->GetContext(); }
  WindowTreeHost* host() { return helper_->GetHost(); }
  ui::EventSink* GetEventSink() { return host()->GetEventSink(); }
  TestScreen* test_screen() { return helper_->GetTestScreen(); }
  client::FocusClient* focus_client() { return helper_->GetFocusClient(); }

 private:
  base::test::TaskEnvironment task_environment_;

#if BUILDFLAG(IS_WIN)
  base::win::ScopedCOMInitializer com_initializer_;
#endif

  bool setup_called_ = false;
  bool teardown_called_ = false;
  std::unique_ptr<AuraTestHelper> helper_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_BASE_H_
