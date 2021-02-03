// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_cursor_factory.h"

#include <wayland-cursor.h>

#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/task_runner_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"

namespace ui {

namespace {

wl_cursor_theme* LoadCursorTheme(const std::string& name,
                                 int size,
                                 wl_shm* shm) {
  // wl_cursor_theme_load() can return nullptr.  We don't check that here but
  // have to be cautious when we actually load the shape.
  return wl_cursor_theme_load((name.empty() ? nullptr : name.c_str()), size,
                              shm);
}

}  // namespace

WaylandCursorFactory::ThemeData::ThemeData() = default;

WaylandCursorFactory::ThemeData::~ThemeData() = default;

WaylandCursorFactory::WaylandCursorFactory(WaylandConnection* connection)
    : connection_(connection) {
  connection_->SetCursorBufferListener(this);
  ReloadThemeCursors();
}

WaylandCursorFactory::~WaylandCursorFactory() = default;

void WaylandCursorFactory::ObserveThemeChanges() {
  auto* cursor_theme_manager = CursorThemeManager::GetInstance();
  DCHECK(cursor_theme_manager);
  cursor_theme_observer_.Observe(cursor_theme_manager);
}

base::Optional<PlatformCursor> WaylandCursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  if (type == mojom::CursorType::kNone)
    return nullptr;  // nullptr is used for the hidden cursor.

  if (current_theme_->cache.count(type) == 0) {
    for (const std::string& name : CursorNamesFromType(type)) {
      wl_cursor* cursor = GetCursorFromTheme(name);
      if (!cursor)
        continue;

      current_theme_->cache[type] =
          base::MakeRefCounted<BitmapCursorOzone>(type, cursor);
      break;
    }
  }
  if (current_theme_->cache.count(type) == 0)
    current_theme_->cache[type] = nullptr;

  // Fall back to the base class implementation if the theme has't provided
  // a shape for the requested type.
  if (current_theme_->cache[type].get() == nullptr)
    return BitmapCursorFactoryOzone::GetDefaultCursor(type);

  return static_cast<PlatformCursor>(current_theme_->cache[type].get());
}

wl_cursor* WaylandCursorFactory::GetCursorFromTheme(const std::string& name) {
  // Possible if the theme could not be loaded.
  if (!current_theme_->theme)
    return nullptr;

  return wl_cursor_theme_get_cursor(current_theme_->theme.get(), name.c_str());
}

void WaylandCursorFactory::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  if (name_ == cursor_theme_name)
    return;

  name_ = cursor_theme_name;
  ReloadThemeCursors();
}

void WaylandCursorFactory::OnCursorThemeSizeChanged(int cursor_theme_size) {
  if (size_ == cursor_theme_size)
    return;

  size_ = cursor_theme_size;
  ReloadThemeCursors();
}

void WaylandCursorFactory::OnCursorBufferAttached(wl_cursor* cursor_data) {
  if (!unloaded_theme_)
    return;
  if (!cursor_data) {
    unloaded_theme_.reset();
    return;
  }
  for (auto& item : current_theme_->cache) {
    if (item.second->platform_data() == cursor_data) {
      // The cursor that has been just attached is from the current theme.  That
      // means that the theme that has been unloaded earlier can now be deleted.
      unloaded_theme_.reset();
      return;
    }
  }
}

void WaylandCursorFactory::ReloadThemeCursors() {
  // If we use any cursor when the theme is reloaded, the one can be only from
  // the theme that is currently used.  As soon as we take the next cursor from
  // the next theme, we will destroy it (see OnCursorBufferAttached() above).
  // If more than one theme has been changed but we didn't take any cursors from
  // them (which is possible if the user played with settings but didn't switch
  // into Chromium), we don't need to track them all.
  if (!unloaded_theme_ && current_theme_ && current_theme_->cache.size() > 0)
    unloaded_theme_ = std::move(current_theme_);

  current_theme_ = std::make_unique<ThemeData>();

  // The task environment is normally not created in tests.  As this factory is
  // part of the platform that is created always and early, posting a task to
  // the pool would fail in many many tests.
  if (!base::ThreadPoolInstance::Get())
    return;

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(LoadCursorTheme, name_, size_, connection_->shm()->get()),
      base::BindOnce(&WaylandCursorFactory::OnThemeLoaded,
                     weak_factory_.GetWeakPtr(), name_, size_));
}

void WaylandCursorFactory::OnThemeLoaded(const std::string& loaded_theme_name,
                                         int loaded_theme_size,
                                         wl_cursor_theme* loaded_theme) {
  if (loaded_theme_name == name_ && loaded_theme_size == size_) {
    // wl_cursor_theme_load() can return nullptr.  We don't check that here but
    // have to be cautious when we actually load the shape.
    current_theme_->theme.reset(loaded_theme);
  }
}

}  // namespace ui
