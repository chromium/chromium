// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace views {

DesktopNativeCursorManager::DesktopNativeCursorManager()
    : cursor_loader_(ui::CursorLoader::Create()) {}

DesktopNativeCursorManager::~DesktopNativeCursorManager() = default;

gfx::NativeCursor DesktopNativeCursorManager::GetInitializedCursor(
    ui::mojom::CursorType type) {
  gfx::NativeCursor cursor(type);
  cursor_loader_->SetPlatformCursor(&cursor);
  return cursor;
}

void DesktopNativeCursorManager::AddHost(aura::WindowTreeHost* host) {
  hosts_.insert(host);
}

void DesktopNativeCursorManager::RemoveHost(aura::WindowTreeHost* host) {
  hosts_.erase(host);
}

void DesktopNativeCursorManager::SetDisplay(
    const display::Display& display,
    wm::NativeCursorManagerDelegate* delegate) {
  cursor_loader_->UnloadAll();
  cursor_loader_->set_rotation(display.rotation());
  cursor_loader_->set_scale(display.device_scale_factor());

  SetCursor(delegate->GetCursor(), delegate);
}

void DesktopNativeCursorManager::SetCursor(
    gfx::NativeCursor cursor,
    wm::NativeCursorManagerDelegate* delegate) {
  gfx::NativeCursor new_cursor = cursor;
  cursor_loader_->SetPlatformCursor(&new_cursor);
  delegate->CommitCursor(new_cursor);

  if (delegate->IsCursorVisible()) {
    for (auto* host : hosts_)
      host->SetCursor(new_cursor);
  }
}

void DesktopNativeCursorManager::SetVisibility(
    bool visible,
    wm::NativeCursorManagerDelegate* delegate) {
  TRACE_EVENT1("ui,input", "DesktopNativeCursorManager::SetVisibility",
               "visible", visible);
  delegate->CommitVisibility(visible);

  if (visible) {
    SetCursor(delegate->GetCursor(), delegate);
  } else {
    gfx::NativeCursor invisible_cursor(ui::mojom::CursorType::kNone);
    cursor_loader_->SetPlatformCursor(&invisible_cursor);
    for (auto* host : hosts_)
      host->SetCursor(invisible_cursor);
  }

  for (auto* host : hosts_)
    host->OnCursorVisibilityChanged(visible);
}

void DesktopNativeCursorManager::SetCursorSize(
    ui::CursorSize cursor_size,
    wm::NativeCursorManagerDelegate* delegate) {
  NOTIMPLEMENTED();
}

void DesktopNativeCursorManager::SetMouseEventsEnabled(
    bool enabled,
    wm::NativeCursorManagerDelegate* delegate) {
  TRACE_EVENT0("ui,input", "DesktopNativeCursorManager::SetMouseEventsEnabled");
  delegate->CommitMouseEventsEnabled(enabled);

  // TODO(erg): In the ash version, we set the last mouse location on Env. I'm
  // not sure this concept makes sense on the desktop.

  SetVisibility(delegate->IsCursorVisible(), delegate);

  for (auto* host : hosts_)
    host->dispatcher()->OnMouseEventsEnableStateChanged(enabled);
}

}  // namespace views
