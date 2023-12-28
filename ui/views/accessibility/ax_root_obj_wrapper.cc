// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_root_obj_wrapper.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
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
    : views::AXAuraObjWrapper(cache), delegate_(delegate) {}

AXRootObjWrapper::~AXRootObjWrapper() = default;

bool AXRootObjWrapper::HasChild(views::AXAuraObjWrapper* child) {
  std::vector<raw_ptr<views::AXAuraObjWrapper, VectorExperimental>> children;
  GetChildren(&children);
  return base::Contains(children, child);
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetParent() {
  return nullptr;
}

void AXRootObjWrapper::GetChildren(
    std::vector<raw_ptr<views::AXAuraObjWrapper, VectorExperimental>>*
        out_children) {
  aura_obj_cache_->GetTopLevelWindows(out_children);
}

void AXRootObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = unique_id_.Get();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  out_node_data->role = ax::mojom::Role::kClient;
#else
  out_node_data->role = ax::mojom::Role::kDesktop;
#endif

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

ui::AXNodeID AXRootObjWrapper::GetUniqueId() const {
  return unique_id_.Get();
}

std::string AXRootObjWrapper::ToString() const {
  return "root";
}

void AXRootObjWrapper::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  delegate_->OnEvent(this, ax::mojom::Event::kLoadComplete);
}
