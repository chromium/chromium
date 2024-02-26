// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/ui/aura/aura_context.h"

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/webui/examples/browser/ui/aura/fill_layout.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/cursor_loader.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/public/activation_client.h"

namespace webui_examples {

namespace {

class FocusRules : public wm::BaseFocusRules {
 public:
  FocusRules() = default;
  FocusRules(const FocusRules&) = delete;
  FocusRules& operator=(const FocusRules&) = delete;
  ~FocusRules() override = default;

 private:
  // wm::BaseFocusRules:
  bool SupportsChildActivation(const aura::Window* window) const override {
    return true;
  }
};

}  // namespace

class AuraContext::NativeCursorManager : public wm::NativeCursorManager {
 public:
  NativeCursorManager() = default;
  ~NativeCursorManager() override = default;

  void AddHost(aura::WindowTreeHost* host) { hosts_.insert(host); }
  void RemoveHost(aura::WindowTreeHost* host) { hosts_.erase(host); }

 private:
  // wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {
    if (cursor_loader_.SetDisplay(display)) {
      SetCursor(delegate->GetCursor(), delegate);
    }
  }

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    gfx::NativeCursor new_cursor = cursor;
    cursor_loader_.SetPlatformCursor(&new_cursor);
    delegate->CommitCursor(new_cursor);
    if (delegate->IsCursorVisible()) {
      for (aura::WindowTreeHost* host : hosts_) {
        host->SetCursor(new_cursor);
      }
    }
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
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

  void SetCursorSize(ui::CursorSize cursor_size,
                     wm::NativeCursorManagerDelegate* delegate) override {
    NOTIMPLEMENTED();
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitMouseEventsEnabled(enabled);
    SetVisibility(delegate->IsCursorVisible(), delegate);
    for (aura::WindowTreeHost* host : hosts_) {
      host->dispatcher()->OnMouseEventsEnableStateChanged(enabled);
    }
  }

  // The set of hosts to notify of changes in cursor state.
  base::flat_set<raw_ptr<aura::WindowTreeHost, CtnExperimental>> hosts_;

  wm::CursorLoader cursor_loader_;
};

AuraContext::ContextualizedWindowTreeHost::ContextualizedWindowTreeHost(
    base::PassKey<AuraContext>,
    AuraContext* context,
    std::unique_ptr<aura::WindowTreeHost> window_tree_host)
    : context_(context), window_tree_host_(std::move(window_tree_host)) {
  context_->InitializeWindowTreeHost(window_tree_host_.get());
}

AuraContext::ContextualizedWindowTreeHost::~ContextualizedWindowTreeHost() {
  context_->UninitializeWindowTreeHost(window_tree_host_.get());
}

AuraContext::AuraContext()
    : screen_(aura::TestScreen::Create(gfx::Size(1024, 768))) {
  DCHECK(!display::Screen::GetScreen());
  display::Screen::SetScreenInstance(screen_.get());
  focus_controller_ = std::make_unique<wm::FocusController>(new FocusRules());
  auto native_cursor_manager = std::make_unique<NativeCursorManager>();
  native_cursor_manager_ = native_cursor_manager.get();
  cursor_manager_ =
      std::make_unique<wm::CursorManager>(std::move(native_cursor_manager));
}

AuraContext::~AuraContext() = default;

std::unique_ptr<AuraContext::ContextualizedWindowTreeHost>
AuraContext::CreateWindowTreeHost() {
  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(gfx::Size(1024, 768));
  auto host = aura::WindowTreeHost::Create(std::move(properties));
  return std::make_unique<ContextualizedWindowTreeHost>(
      base::PassKey<AuraContext>(), this, std::move(host));
}

void AuraContext::InitializeWindowTreeHost(aura::WindowTreeHost* host) {
  host->InitHost();
  aura::client::SetFocusClient(host->window(), focus_controller_.get());
  wm::SetActivationClient(host->window(), focus_controller_.get());
  host->window()->AddPreTargetHandler(focus_controller_.get());
  host->window()->SetLayoutManager(
      std::make_unique<FillLayout>(host->window()));

  native_cursor_manager_->AddHost(host);
  aura::client::SetCursorClient(host->window(), cursor_manager_.get());
}

void AuraContext::UninitializeWindowTreeHost(aura::WindowTreeHost* host) {
  native_cursor_manager_->RemoveHost(host);
  host->window()->RemovePreTargetHandler(focus_controller_.get());
}

}  // namespace webui_examples
