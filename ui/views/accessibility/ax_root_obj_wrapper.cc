// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_root_obj_wrapper.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_window_obj_wrapper.h"

AXRootObjWrapper::AXRootObjWrapper(views::AXAuraObjCache::Delegate* delegate,
                                   views::AXAuraObjCache* cache)
    : AXAuraObjWrapper(cache), delegate_(delegate) {
  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->AddObserver(this);
}

AXRootObjWrapper::~AXRootObjWrapper() {
  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->RemoveObserver(this);
}

bool AXRootObjWrapper::HasChild(views::AXAuraObjWrapper* child) {
  std::vector<views::AXAuraObjWrapper*> children;
  GetChildren(&children);
  return base::Contains(children, child);
}

bool AXRootObjWrapper::IsIgnored() {
  return false;
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetParent() {
  return nullptr;
}

void AXRootObjWrapper::GetChildren(
    std::vector<views::AXAuraObjWrapper*>* out_children) {
  aura_obj_cache_->GetTopLevelWindows(out_children);
}

void AXRootObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = unique_id_.Get();
  out_node_data->role = ax::mojom::Role::kDesktop;

  display::Screen* screen = display::Screen::GetScreen();
  if (!screen)
    return;

  const display::Display& display = screen->GetPrimaryDisplay();

  out_node_data->relative_bounds.bounds =
      gfx::RectF(display.bounds().width(), display.bounds().height());

  // Utilize the display bounds to figure out if this screen is in landscape or
  // portrait. We use this rather than |rotation| because some devices default
  // to landscape, some in portrait. Encode landscape as horizontal state,
  // portrait as vertical state.
  if (display.bounds().width() > display.bounds().height())
    out_node_data->AddState(ax::mojom::State::kHorizontal);
  else
    out_node_data->AddState(ax::mojom::State::kVertical);
}

int32_t AXRootObjWrapper::GetUniqueId() const {
  return unique_id_.Get();
}

void AXRootObjWrapper::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  delegate_->OnEvent(this, ax::mojom::Event::kLocationChanged);
}
