// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor_factory.h"

#include <wayland-cursor.h>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"

using ::testing::Values;

namespace ui {

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
  ~DryRunningWaylandCursorFactory() override {
    // All BitmapCursor objects held by the parent class must be destroyed
    // before `cursors_` or the tests will fail with a dangling raw_ptr error.
    FlushThemeCache(true);
  }

 protected:
  // Pretends to load a cursor by creating an empty wl_cursor.
  wl_cursor* GetCursorFromTheme(wl_cursor_theme* theme,
                                const std::string& name) override {
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

class WaylandCursorFactoryTest : public WaylandTestSimple {
 protected:
  void TriggerThemeLoad(WaylandCursorFactory* cursor_factory,
                        float scale = 1.0) {
    ASSERT_FALSE(theme_loading_in_progress_);
    theme_loading_in_progress_ = true;
    cursor_factory->GetThemeForScale(scale)->AddThemeLoadedCallback(
        base::BindOnce(&WaylandCursorFactoryTest::OnThemeLoaded,
                       weak_factory_.GetWeakPtr()));
  }

  void WaitForThemeLoaded() {
    if (!theme_loading_in_progress_) {
      return;
    }
    base::RunLoop loop;
    ASSERT_FALSE(loop_quit_closure_);
    loop_quit_closure_ = loop.QuitClosure();
    loop.Run();
    ASSERT_FALSE(theme_loading_in_progress_);
  }

 private:
  void OnThemeLoaded(wl_cursor_theme*) {
    ASSERT_TRUE(theme_loading_in_progress_);
    theme_loading_in_progress_ = false;
    if (loop_quit_closure_) {
      std::move(loop_quit_closure_).Run();
    }
  }

  base::OnceClosure loop_quit_closure_;
  bool theme_loading_in_progress_ = false;
  base::WeakPtrFactory<WaylandCursorFactoryTest> weak_factory_{this};
};

// Tests that the factory holds the cursor theme until a buffer taken from it
// released.
TEST_F(WaylandCursorFactoryTest, RetainOldThemeUntilNewBufferIsAttached) {
  std::unique_ptr<WaylandCursorFactory> cursor_factory =
      std::make_unique<DryRunningWaylandCursorFactory>(connection_.get());

  // The unloaded theme should not be set.
  EXPECT_EQ(cursor_factory->unloaded_theme_, nullptr);

  TriggerThemeLoad(cursor_factory.get());
  WaitForThemeLoaded();

  // Trigger theme reload and ensure that the theme instance has changed.
  // As we didn't request any buffers, the unloaded theme should not be held.
  // TODO(crbug.com/40861116): the current method here compares addresses of the
  // objects before and after the reload, and expects the address to change,
  // which means that a new object has been allocated.  Technically that is not
  // guaranteed.  Probably a better way should be invented.
  {
    auto* const current_theme_object =
        cursor_factory->theme_cache_->begin()->second.get()->theme();
    cursor_factory->OnCursorThemeNameChanged("Theme1");
    TriggerThemeLoad(cursor_factory.get());
    EXPECT_EQ(cursor_factory->theme_cache_->size(), 1U);
    EXPECT_NE(cursor_factory->theme_cache_->begin()->second.get()->theme(),
              current_theme_object);
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
    auto* const used_current_theme = cursor_factory->theme_cache_.get();
    auto const cursor =
        cursor_factory->GetDefaultCursor(mojom::CursorType::kPointer);
    EXPECT_NE(cursor, nullptr);
    EXPECT_GT(used_current_theme->size(), 0U);

    for (const auto* name : {"Theme2", "Theme3", "Theme2", "Theme3"}) {
      cursor_factory->OnCursorThemeNameChanged(name);
      TriggerThemeLoad(cursor_factory.get());

      auto* const new_current_theme = cursor_factory->theme_cache_.get();
      ASSERT_EQ(new_current_theme->size(), 1U);
      EXPECT_EQ(new_current_theme->begin()->second->cache.size(), 0U);
      EXPECT_NE(new_current_theme, used_current_theme);
      EXPECT_EQ(cursor_factory->unloaded_theme_.get(), used_current_theme);

      WaitForThemeLoaded();
    }

    cursor_factory->OnCursorBufferAttached(
        static_cast<wl_cursor*>(WaylandAsyncCursor::FromPlatformCursor(cursor)
                                    ->bitmap_cursor()
                                    ->platform_data()));
    EXPECT_EQ(cursor_factory->unloaded_theme_.get(), used_current_theme);
  }

  // Finally, tell the factory that we have attached a buffer from the current
  // theme.  This time the old theme held since a while ago should be freed.
  {
    auto const cursor =
        cursor_factory->GetDefaultCursor(mojom::CursorType::kPointer);
    EXPECT_NE(cursor, nullptr);

    cursor_factory->OnCursorBufferAttached(
        static_cast<wl_cursor*>(WaylandAsyncCursor::FromPlatformCursor(cursor)
                                    ->bitmap_cursor()
                                    ->platform_data()));

    EXPECT_EQ(cursor_factory->unloaded_theme_.get(), nullptr);
  }
}

// Tests that the factory keeps the caches when either cursor size or buffer
// scale are changed, and only resets them when the theme is changed.
TEST_F(WaylandCursorFactoryTest, CachesSizesUntilThemeNameIsChanged) {
  std::unique_ptr<WaylandCursorFactory> cursor_factory =
      std::make_unique<DryRunningWaylandCursorFactory>(connection_.get());

  // The unloaded theme should not be set.
  EXPECT_EQ(cursor_factory->unloaded_theme_, nullptr);

  TriggerThemeLoad(cursor_factory.get());
  WaitForThemeLoaded();

  // Trigger theme reload and ensure that the theme instance has changed.
  // As we didn't request any buffers, the unloaded theme should not be held.
  // TODO(crbug.com/40861116): the current method here compares addresses of the
  // objects before and after the reload, and expects the address to change,
  // which means that a new object has been allocated.  Technically that is not
  // guaranteed.  Probably a better way should be invented.
  {
    auto* const current_theme_object =
        cursor_factory->theme_cache_->begin()->second.get()->theme();
    cursor_factory->OnCursorThemeNameChanged("Theme1");
    TriggerThemeLoad(cursor_factory.get());
    EXPECT_EQ(cursor_factory->theme_cache_->size(), 1U);
    EXPECT_NE(cursor_factory->theme_cache_->begin()->second.get()->theme(),
              current_theme_object);
    EXPECT_EQ(cursor_factory->unloaded_theme_, nullptr);

    WaitForThemeLoaded();
  }

  // Now request changing the buffer scale and the cursor size in a number of
  // combinations.  Each combination of scale and size should create an entry in
  // the cache keyed as (size * rounded scale), so some combinations overlap,
  // and in the end there should be 5 entries.  Setting a new size or a new
  // scale should never trigger the theme reload.
  EXPECT_EQ(cursor_factory->size_, 24);
  {
    for (const auto size : {24, 36, 48}) {
      for (const auto scale : {1.0, 1.5, 2.0}) {
        cursor_factory->OnCursorThemeSizeChanged(size);
        TriggerThemeLoad(cursor_factory.get(), scale);

        EXPECT_EQ(cursor_factory->theme_cache_->count(
                      static_cast<int>(cursor_factory->GetScaledSize(scale))),
                  1U);
        WaitForThemeLoaded();
      }
    }
    EXPECT_EQ(cursor_factory->theme_cache_->size(), 5U);
  }

  // Next, "take" a cursor and then trigger the theme reload.  The factory
  // should reset the theme cache and have only one theme held because of the
  // cursor used currently.
  {
    auto const cursor =
        cursor_factory->GetDefaultCursor(mojom::CursorType::kPointer);
    EXPECT_NE(cursor, nullptr);
    EXPECT_EQ(cursor_factory->theme_cache_->size(), 5U);

    cursor_factory->OnCursorThemeNameChanged("Theme2");
    TriggerThemeLoad(cursor_factory.get());

    ASSERT_EQ(cursor_factory->theme_cache_->size(), 1U);
    auto* const new_current_theme =
        cursor_factory->theme_cache_->begin()->second.get();
    EXPECT_EQ(new_current_theme->cache.size(), 0U);
    EXPECT_NE(cursor_factory->unloaded_theme_.get(), nullptr);

    WaitForThemeLoaded();

    cursor_factory->OnCursorBufferAttached(
        static_cast<wl_cursor*>(WaylandAsyncCursor::FromPlatformCursor(cursor)
                                    ->bitmap_cursor()
                                    ->platform_data()));
    EXPECT_NE(cursor_factory->unloaded_theme_.get(), nullptr);
  }

  // Finally, tell the factory that we have attached a buffer from the current
  // theme.  This time the old theme held since a while ago should be freed.
  {
    auto const cursor =
        cursor_factory->GetDefaultCursor(mojom::CursorType::kPointer);
    EXPECT_NE(cursor, nullptr);

    cursor_factory->OnCursorBufferAttached(
        static_cast<wl_cursor*>(WaylandAsyncCursor::FromPlatformCursor(cursor)
                                    ->bitmap_cursor()
                                    ->platform_data()));

    EXPECT_EQ(cursor_factory->unloaded_theme_.get(), nullptr);
  }
}

}  // namespace ui
