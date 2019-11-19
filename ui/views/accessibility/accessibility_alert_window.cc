// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/accessibility_alert_window.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace views {

AccessibilityAlertWindow::AccessibilityAlertWindow(aura::Window* parent,
                                                   views::AXAuraObjCache* cache)
    : cache_(cache) {
  CHECK(parent);
  alert_window_ = std::make_unique<aura::Window>(
      nullptr, aura::client::WINDOW_TYPE_UNKNOWN);
  alert_window_->set_owned_by_parent(false);
  alert_window_->Init(ui::LayerType::LAYER_NOT_DRAWN);
  alert_window_->SetProperty(ui::kAXRoleOverride, ax::mojom::Role::kAlert);
  parent->AddChild(alert_window_.get());
  observer_.Add(aura::Env::GetInstance());
}

AccessibilityAlertWindow::~AccessibilityAlertWindow() = default;

void AccessibilityAlertWindow::HandleAlert(const std::string& alert_string) {
  if (!alert_window_->parent())
    return;

  alert_window_->SetTitle(base::UTF8ToUTF16(alert_string));
  cache_->FireEvent(cache_->GetOrCreate(alert_window_.get()),
                    ax::mojom::Event::kAlert);
}

void AccessibilityAlertWindow::OnWillDestroyEnv() {
  observer_.RemoveAll();
  alert_window_.reset();
}
}  // namespace views
