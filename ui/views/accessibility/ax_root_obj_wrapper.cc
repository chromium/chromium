// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_root_obj_wrapper.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_window_obj_wrapper.h"

AXRootObjWrapper::AXRootObjWrapper(views::AXAuraObjCache::Delegate* delegate)
    : alert_window_(std::make_unique<aura::Window>(nullptr)),
      delegate_(delegate) {
  alert_window_->Init(ui::LAYER_NOT_DRAWN);
#if !defined(IS_CHROMECAST)
  aura::Env::GetInstance()->AddObserver(this);

  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->AddObserver(this);
#endif
}

AXRootObjWrapper::~AXRootObjWrapper() {
#if !defined(IS_CHROMECAST)
  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->RemoveObserver(this);

  // If alert_window_ is nullptr already, that means OnWillDestroyEnv
  // was already called, so we shouldn't call RemoveObserver(this) again.
  if (!alert_window_)
    return;

  aura::Env::GetInstance()->RemoveObserver(this);
#endif
  alert_window_.reset();
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetAlertForText(
    const std::string& text) {
  alert_window_->SetTitle(base::UTF8ToUTF16((text)));
  views::AXWindowObjWrapper* window_obj =
      static_cast<views::AXWindowObjWrapper*>(
          views::AXAuraObjCache::GetInstance()->GetOrCreate(
              alert_window_.get()));
  window_obj->set_is_alert(true);
  return window_obj;
}

bool AXRootObjWrapper::HasChild(views::AXAuraObjWrapper* child) {
  std::vector<views::AXAuraObjWrapper*> children;
  GetChildren(&children);
  return base::ContainsValue(children, child);
}

bool AXRootObjWrapper::IsIgnored() {
  return false;
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetParent() {
  return NULL;
}

void AXRootObjWrapper::GetChildren(
    std::vector<views::AXAuraObjWrapper*>* out_children) {
  views::AXAuraObjCache::GetInstance()->GetTopLevelWindows(out_children);
  out_children->push_back(
      views::AXAuraObjCache::GetInstance()->GetOrCreate(alert_window_.get()));
}

void AXRootObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = unique_id_.Get();
  out_node_data->role = ax::mojom::Role::kDesktop;

#if !defined(IS_CHROMECAST)
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen)
    return;

  const display::Display& display = screen->GetPrimaryDisplay();

  // Utilize the display bounds to figure out if this screen is in landscape or
  // portrait. We use this rather than |rotation| because some devices default
  // to landscape, some in portrait. Encode landscape as horizontal state,
  // portrait as vertical state.
  if (display.bounds().width() > display.bounds().height())
    out_node_data->AddState(ax::mojom::State::kHorizontal);
  else
    out_node_data->AddState(ax::mojom::State::kVertical);
#endif
}

const ui::AXUniqueId& AXRootObjWrapper::GetUniqueId() const {
  return unique_id_;
}

void AXRootObjWrapper::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  delegate_->OnEvent(this, ax::mojom::Event::kLocationChanged);
}

void AXRootObjWrapper::OnWindowInitialized(aura::Window* window) {}

void AXRootObjWrapper::OnWillDestroyEnv() {
  alert_window_.reset();
  aura::Env::GetInstance()->RemoveObserver(this);
}
