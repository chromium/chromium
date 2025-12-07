// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_TEST_BASE_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_TEST_BASE_H_

#include <memory>

#include "ui/views/test/views_test_base.h"

namespace views {

class NativeViewHost;
class NativeViewHostWrapper;
class Widget;
class WidgetDelegate;

namespace test {

// Base class for NativeViewHost tests on different platforms.
class NativeViewHostTestBase : public ViewsTestBase {
 public:
  NativeViewHostTestBase();

  NativeViewHostTestBase(const NativeViewHostTestBase&) = delete;
  NativeViewHostTestBase& operator=(const NativeViewHostTestBase&) = delete;

  ~NativeViewHostTestBase() override;

  // testing::Test:
  void TearDown() override;

  // Create the |toplevel_| widget.
  void CreateTopLevel(WidgetDelegate* widget_delegate = nullptr);

  // Create a testing |host_| that tracks destructor calls.
  void CreateTestingHost();

  // The number of times a host created by CreateHost() has been destroyed.
  int host_destroyed_count() { return host_destroyed_count_; }
  void ResetHostDestroyedCount() { host_destroyed_count_ = 0; }

  // Create a child widget whose native parent is |native_parent_view|, uses
  // |contents_view|, and is attached to |host| which is added as a child to
  // |parent_view|. This effectively borrows the native content view from a
  // newly created child Widget, and attaches it to |host|.
  std::unique_ptr<Widget> CreateChildForHost(gfx::NativeView native_parent_view,
                                             View* parent_view,
                                             View* contents_view,
                                             NativeViewHost* host);

  Widget* toplevel() { return toplevel_.get(); }
  void DestroyTopLevel();

  NativeViewHost* host() { return host_.get(); }
  void DestroyHost();
  NativeViewHost* ReleaseHost();

  NativeViewHostWrapper* GetNativeWrapper();

 protected:
  int on_mouse_pressed_called_count() { return on_mouse_pressed_called_count_; }

 private:
  class NativeViewHostTesting;

  std::unique_ptr<Widget> toplevel_;
  std::unique_ptr<NativeViewHost> host_;
  int host_destroyed_count_ = 0;
  int on_mouse_pressed_called_count_ = 0;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_TEST_BASE_H_
