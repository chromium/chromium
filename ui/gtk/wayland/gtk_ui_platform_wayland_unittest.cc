// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/wayland/gtk_ui_platform_wayland.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/linux/linux_ui_delegate_stub.h"

namespace gtk {

namespace {

// Holds on to the export callback so that tests decide when the parent handle
// arrives.
class TestLinuxUiDelegate : public ui::LinuxUiDelegateStub {
 public:
  TestLinuxUiDelegate() = default;
  ~TestLinuxUiDelegate() override = default;

  void ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) override {
    export_callback_ = std::move(callback);
  }

  bool HasExportCallback() const { return !export_callback_.is_null(); }

  void RunExportCallback(std::string handle) {
    std::move(export_callback_).Run(std::move(handle));
  }

 private:
  base::OnceCallback<void(std::string)> export_callback_;
};

class GtkUiPlatformWaylandTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestLinuxUiDelegate delegate_;
  GtkUiPlatformWayland platform_;
};

constexpr gfx::AcceleratedWidget kParent = 1;

// Reads the "visible" property through GObject, since GTK functions are only
// reachable from inside the gtk component.
bool IsVisible(GtkWidget* widget) {
  gboolean visible = FALSE;
  g_object_get(widget, "visible", &visible, nullptr);
  return visible;
}

GtkWindow* AsWindow(GtkWidget* widget) {
  return reinterpret_cast<GtkWindow*>(widget);
}

}  // namespace

// Tests that a window is not mapped before its transient parent is set.
TEST_F(GtkUiPlatformWaylandTest, ShowWaitsForTransientParent) {
  GtkWidget* window = GtkToplevelWindowNew();
  absl::Cleanup destroy_window = [window] { GtkWindowDestroy(window); };

  platform_.SetGtkWidgetTransientFor(window, kParent);
  platform_.ShowGtkWindow(AsWindow(window));
  EXPECT_FALSE(IsVisible(window));

  // An empty handle means that exporting the parent failed. The window must
  // still be shown.
  ASSERT_TRUE(delegate_.HasExportCallback());
  delegate_.RunExportCallback(std::string());
  EXPECT_TRUE(IsVisible(window));
}

// Tests that a window that was never asked to be shown stays hidden when its
// parent handle arrives.
TEST_F(GtkUiPlatformWaylandTest, HandleWithoutShowDoesNotMap) {
  GtkWidget* window = GtkToplevelWindowNew();
  absl::Cleanup destroy_window = [window] { GtkWindowDestroy(window); };

  platform_.SetGtkWidgetTransientFor(window, kParent);

  ASSERT_TRUE(delegate_.HasExportCallback());
  delegate_.RunExportCallback(std::string());
  EXPECT_FALSE(IsVisible(window));

  // Without a pending transient, ShowGtkWindow() maps the window right away.
  platform_.ShowGtkWindow(AsWindow(window));
  EXPECT_TRUE(IsVisible(window));
}

// Tests that destroying a window while its parent is being exported is safe.
TEST_F(GtkUiPlatformWaylandTest, DestroyBeforeHandle) {
  GtkWidget* window = GtkToplevelWindowNew();
  platform_.SetGtkWidgetTransientFor(window, kParent);
  platform_.ShowGtkWindow(AsWindow(window));
  GtkWindowDestroy(window);

  ASSERT_TRUE(delegate_.HasExportCallback());
  delegate_.RunExportCallback(std::string());
}

}  // namespace gtk
