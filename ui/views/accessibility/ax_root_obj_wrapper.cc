// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_root_obj_wrapper.h"

#include <utility>

#include "base/containers/contains.h"
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

#if defined(OS_CHROMEOS)
namespace {

constexpr char kAXLacrosAppId[] = "AXLacrosApp";

// A simple wrapper object that can reference another tree via app id.
class AXHostAuraObj : public views::AXAuraObjWrapper {
 public:
  AXHostAuraObj(views::AXAuraObjCache* cache,
                const std::string& child_app_id,
                views::AXAuraObjWrapper* parent)
      : views::AXAuraObjWrapper(cache),
        child_app_id_(child_app_id),
        parent_(parent) {}

  ~AXHostAuraObj() override = default;

  // views::AXAuraObjWrapper:
  views::AXAuraObjWrapper* GetParent() override { return parent_; }

  void GetChildren(std::vector<AXAuraObjWrapper*>* out_children) override {}

  void Serialize(ui::AXNodeData* out_node_data) override {
    out_node_data->id = GetUniqueId();
    out_node_data->AddStringAttribute(
        ax::mojom::StringAttribute::kChildTreeNodeAppId, child_app_id_);
    out_node_data->role = ax::mojom::Role::kClient;
  }

  ui::AXNodeID GetUniqueId() const override { return unique_id_.Get(); }
  std::string ToString() const override { return std::string(); }

 private:
  const std::string child_app_id_;
  views::AXAuraObjWrapper* parent_;
  const ui::AXUniqueId unique_id_;
};

}  // namespace
#endif  // defined(OS_CHROMEOS)

AXRootObjWrapper::AXRootObjWrapper(views::AXAuraObjCache::Delegate* delegate,
                                   views::AXAuraObjCache* cache)
    : views::AXAuraObjWrapper(cache), delegate_(delegate) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto lacros = std::make_unique<AXHostAuraObj>(cache, kAXLacrosAppId, this);
  lacros_host_ = lacros.get();
  cache->CreateOrReplace(std::move(lacros));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

AXRootObjWrapper::~AXRootObjWrapper() = default;

bool AXRootObjWrapper::HasChild(views::AXAuraObjWrapper* child) {
  std::vector<views::AXAuraObjWrapper*> children;
  GetChildren(&children);
  return base::Contains(children, child);
}

views::AXAuraObjWrapper* AXRootObjWrapper::GetParent() {
  return nullptr;
}

void AXRootObjWrapper::GetChildren(
    std::vector<views::AXAuraObjWrapper*>* out_children) {
  aura_obj_cache_->GetTopLevelWindows(out_children);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Add a host for LaCrOS.
  out_children->push_back(lacros_host_);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void AXRootObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = unique_id_.Get();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  out_node_data->role = ax::mojom::Role::kClient;
  out_node_data->AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                    kAXLacrosAppId);
#else
  out_node_data->role = ax::mojom::Role::kDesktop;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
