// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ui {
namespace {

typedef std::map<gfx::AcceleratedWidget,AcceleratedWidgetMac*>
    WidgetToHelperMap;
base::LazyInstance<WidgetToHelperMap>::DestructorAtExit g_widget_to_helper_map;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratedWidgetMac

AcceleratedWidgetMac::AcceleratedWidgetMac() : view_(nullptr) {
  // Use a sequence number as the accelerated widget handle that we can use
  // to look up the internals structure.
  static uint64_t last_sequence_number = 0;
  native_widget_ = ++last_sequence_number;
  g_widget_to_helper_map.Pointer()->insert(
      std::make_pair(native_widget_, this));
}

AcceleratedWidgetMac::~AcceleratedWidgetMac() {
  DCHECK(!view_);
  g_widget_to_helper_map.Pointer()->erase(native_widget_);
}

void AcceleratedWidgetMac::SetNSView(AcceleratedWidgetMacNSView* view) {
  DCHECK(view && !view_);
  view_ = view;
}

void AcceleratedWidgetMac::ResetNSView() {
  last_ca_layer_params_valid_ = false;
  view_ = nullptr;
}

const gfx::CALayerParams* AcceleratedWidgetMac::GetCALayerParams() const {
  if (!last_ca_layer_params_valid_)
    return nullptr;
  return &last_ca_layer_params_;
}

bool AcceleratedWidgetMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  if (!last_ca_layer_params_valid_)
    return false;
  // TODO(danakj): We should avoid lossy conversions to integer DIPs.
  gfx::Size last_swap_size_dip = gfx::ToFlooredSize(gfx::ConvertSizeToDips(
      last_ca_layer_params_.pixel_size, last_ca_layer_params_.scale_factor));
  return last_swap_size_dip == dip_size;
}

// static
AcceleratedWidgetMac* AcceleratedWidgetMac::Get(gfx::AcceleratedWidget widget) {
  WidgetToHelperMap::const_iterator found =
      g_widget_to_helper_map.Pointer()->find(widget);
  // This can end up being accessed after the underlying widget has been
  // destroyed, but while the ui::Compositor is still being destroyed.
  // Return NULL in these cases.
  if (found == g_widget_to_helper_map.Pointer()->end())
    return nullptr;
  return found->second;
}

void AcceleratedWidgetMac::SetSuspended(bool is_suspended) {
  is_suspended_ = is_suspended;
}

void AcceleratedWidgetMac::UpdateCALayerTree(
    const gfx::CALayerParams& ca_layer_params) {
  if (is_suspended_)
    return;

  last_ca_layer_params_valid_ = true;
  last_ca_layer_params_ = ca_layer_params;
  if (view_)
    view_->AcceleratedWidgetCALayerParamsUpdated();
}


}  // namespace ui
