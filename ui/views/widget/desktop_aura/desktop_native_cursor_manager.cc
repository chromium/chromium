// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"

#include "base/trace_event/trace_event.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/wm/core/cursor_loader.h"

namespace views {

DesktopNativeCursorManager::DesktopNativeCursorManager() {
  aura::client::SetCursorShapeClient(&cursor_loader_);
}

DesktopNativeCursorManager::~DesktopNativeCursorManager() {
  aura::client::SetCursorShapeClient(nullptr);
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
  if (cursor_loader_.SetDisplay(display)) {
    SetCursor(delegate->GetCursor(), delegate);
  }
}

void DesktopNativeCursorManager::SetCursor(
    gfx::NativeCursor cursor,
    wm::NativeCursorManagerDelegate* delegate) {
  gfx::NativeCursor new_cursor = cursor;
  cursor_loader_.SetPlatformCursor(&new_cursor);
  delegate->CommitCursor(new_cursor);

  if (delegate->IsCursorVisible()) {
    for (aura::WindowTreeHost* host : hosts_) {
      host->SetCursor(new_cursor);
    }
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
    cursor_loader_.SetPlatformCursor(&invisible_cursor);
    for (aura::WindowTreeHost* host : hosts_) {
      host->SetCursor(invisible_cursor);
    }
  }

  for (aura::WindowTreeHost* host : hosts_) {
    host->OnCursorVisibilityChanged(visible);
  }
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

  for (aura::WindowTreeHost* host : hosts_) {
    host->dispatcher()->OnMouseEventsEnableStateChanged(enabled);
  }
}

void DesktopNativeCursorManager::InitCursorSizeObserver(
    wm::NativeCursorManagerDelegate* delegate) {
  NOTREACHED();
}

}  // namespace views
