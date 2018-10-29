// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_BASE_H_
#define UI_AURA_TEST_AURA_TEST_BASE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/test/aura_test_helper.h"

namespace ws {
namespace mojom {
class WindowTreeClient;
}
}

namespace aura {
class Window;
class WindowDelegate;
class WindowTreeClientDelegate;

namespace client {
class FocusClient;
}

namespace test {

class AuraTestContextFactory;

// A base class for aura unit tests.
// TODO(beng): Instances of this test will create and own a RootWindow.
class AuraTestBase : public testing::Test, public WindowTreeClientDelegate {
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
  void set_window_tree_client_delegate(
      WindowTreeClientDelegate* window_tree_client_delegate) {
    window_tree_client_delegate_ = window_tree_client_delegate;
  }

  // Turns on mus with a test WindowTree. Must be called before SetUp().
  void EnableMusWithTestWindowTree();

  // Deletes the WindowTreeClient now. Normally the WindowTreeClient is deleted
  // at the right time and there is no need to call this. This is provided for
  // testing shutdown ordering.
  void DeleteWindowTreeClient();

  // Used to configure the backend. This is exposed to make parameterized tests
  // easy to write. This *must* be called from SetUp().
  void ConfigureEnvMode(Env::Mode mode);

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

  TestWindowTree* window_tree() { return helper_->window_tree(); }
  WindowTreeClient* window_tree_client_impl() {
    return helper_->window_tree_client();
  }
  ws::mojom::WindowTreeClient* window_tree_client();

  // WindowTreeClientDelegate:
  void OnEmbed(std::unique_ptr<WindowTreeHostMus> window_tree_host) override;
  void OnUnembed(Window* root) override;
  void OnEmbedRootDestroyed(WindowTreeHostMus* window_tree_host) override;
  void OnLostConnection(WindowTreeClient* client) override;
  PropertyConverter* GetPropertyConverter() override;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Only used for mus, initially set to this, but may be reset.
  WindowTreeClientDelegate* window_tree_client_delegate_;

  Env::Mode env_mode_ = Env::Mode::LOCAL;
  bool setup_called_ = false;
  bool teardown_called_ = false;
  PropertyConverter property_converter_;
  std::unique_ptr<AuraTestHelper> helper_;
  std::unique_ptr<AuraTestContextFactory> mus_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuraTestBase);
};

// Use as a base class for tests that want to target both backends.
class AuraTestBaseWithType : public AuraTestBase,
                             public ::testing::WithParamInterface<Env::Mode> {
 public:
  AuraTestBaseWithType();
  ~AuraTestBaseWithType() override;

  // AuraTestBase:
  void SetUp() override;

 private:
  bool setup_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(AuraTestBaseWithType);
};

class AuraTestBaseMus : public AuraTestBase {
 public:
  AuraTestBaseMus();
  ~AuraTestBaseMus() override;

  // AuraTestBase:
  void SetUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuraTestBaseMus);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_BASE_H_
