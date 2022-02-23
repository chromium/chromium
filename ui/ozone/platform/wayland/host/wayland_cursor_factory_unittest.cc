// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor_factory.h"

#include <wayland-cursor.h>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {

namespace {

// Overrides WaylandCursorFactory::GetCursorFromTheme() to pretend that cursors
// are really loaded.
//
// Note: the parent class does real Wayland calls that try to load the theme.
// That may succeed or fail on the bot, but as GetCursorTheme() is overridden,
// we ignore the result.
class DryRunningWaylandCursorFactory : public WaylandCursorFactory {
 public:
  explicit DryRunningWaylandCursorFactory(WaylandConnection* connection)
      : WaylandCursorFactory(connection) {}
  DryRunningWaylandCursorFactory(const DryRunningWaylandCursorFactory&) =
      delete;
  DryRunningWaylandCursorFactory& operator=(
      const DryRunningWaylandCursorFactory&) = delete;
  ~DryRunningWaylandCursorFactory() override = default;

 protected:
  // Pretends to load a cursor by creating an empty wl_cursor.
  wl_cursor* GetCursorFromTheme(const std::string& name) override {
    if (cursors_.count(name) == 0) {
      cursors_[name] = std::make_unique<wl_cursor>();
      cursors_[name]->image_count = 0;
      cursors_[name]->images = nullptr;
      cursors_[name]->name = nullptr;
    }
    return cursors_[name].get();
  }

 private:
  base::flat_map<std::string, std::unique_ptr<wl_cursor>> cursors_;
};

}  // namespace

class WaylandCursorFactoryTest : public WaylandTest,
                                 public CursorFactoryObserver {
 public:
  WaylandCursorFactoryTest() = default;

  // CursorFactoryObserver:
  void OnThemeLoaded() override {
    ASSERT_TRUE(loop_quit_closure_);
    std::move(loop_quit_closure_).Run();
  }

 protected:
  void WaitForThemeLoaded() {
    base::RunLoop loop;
    ASSERT_FALSE(loop_quit_closure_);
    loop_quit_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  base::OnceClosure loop_quit_closure_;
};

// Tests that the factory holds the cursor theme until a buffer taken from it
// released.
TEST_P(WaylandCursorFactoryTest, RetainOldThemeUntilNewBufferIsAttached) {
  std::unique_ptr<WaylandCursorFactory> cursor_factory =
      std::make_unique<DryRunningWaylandCursorFactory>(connection_.get());
  cursor_factory->AddObserver(this);

  // The default theme should be loaded right away.  The unloaded theme should
  // not be set.
  EXPECT_NE(cursor_factory->current_theme_, nullptr);
  EXPECT_EQ(cursor_factory->unloaded_theme_, nullptr);

  WaitForThemeLoaded();

  // Trigger theme reload and ensure that the theme instance has changed.
  // As we didn't request any buffers, the unloaded theme should not be held.
  {
    auto* const current_theme = cursor_factory->current_theme_.get();
    cursor_factory->OnCursorThemeNameChanged("Theme1");
    EXPECT_NE(cursor_factory->current_theme_, nullptr);
    EXPECT_NE(cursor_factory->current_theme_.get(), current_theme);
    EXPECT_EQ(cursor_factory->unloaded_theme_, nullptr);

    WaitForThemeLoaded();
  }

  // Now request some buffer, and while "holding" it (i.e., not notifying the
  // factory about attaching any other buffer), reload the theme a couple times.
  // This time the unloaded theme should be set and survive these reloads.
  // In the end, tell the factory that we have attached a buffer belonging to
  // the cursor from the "unloaded" theme.  This must not trigger unloading of
  // that theme.
  {
    auto* const current_theme = cursor_factory->current_theme_.get();
    auto const cursor =
        cursor_factory->GetDefaultCursor(mojom::CursorType::kPointer);
    EXPECT_NE(cursor, nullptr);
    EXPECT_GT(cursor_factory->current_theme_->cache.size(), 0U);

    cursor_factory->OnCursorThemeNameChanged("Theme2");

    EXPECT_EQ(cursor_factory->current_theme_->cache.size(), 0U);
    EXPECT_NE(cursor_factory->current_theme_, nullptr);
    EXPECT_NE(cursor_factory->current_theme_.get(), current_theme);
    EXPECT_EQ(cursor_factory->unloaded_theme_.get(), current_theme);

    WaitForThemeLoaded();

    cursor_factory->OnCursorThemeNameChanged("Theme3");

    EXPECT_EQ(cursor_factory->current_theme_->cache.size(), 0U);
    EXPECT_NE(cursor_factory->current_theme_, nullptr);
    EXPECT_NE(cursor_factory->current_theme_.get(), current_theme);
    EXPECT_EQ(cursor_factory->unloaded_theme_.get(), current_theme);

    WaitForThemeLoaded();

    cursor_factory->OnCursorBufferAttached(static_cast<wl_cursor*>(
        BitmapCursor::FromPlatformCursor(cursor)->platform_data()));
    EXPECT_EQ(cursor_factory->unloaded_theme_.get(), current_theme);
  }

  // Finally, tell the factory that we have attached a buffer from the current
  // theme.  This time the old theme held since a while ago should be freed.
  {
    auto const cursor =
        cursor_factory->GetDefaultCursor(mojom::CursorType::kPointer);
    EXPECT_NE(cursor, nullptr);

    cursor_factory->OnCursorBufferAttached(static_cast<wl_cursor*>(
        BitmapCursor::FromPlatformCursor(cursor)->platform_data()));

    EXPECT_EQ(cursor_factory->unloaded_theme_.get(), nullptr);
  }
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandCursorFactoryTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kStable}));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandCursorFactoryTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kV6}));

}  // namespace ui
