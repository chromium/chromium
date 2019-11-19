// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/tooltip_manager_mac.h"

#include "base/no_destructor.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/mac/coordinate_conversion.h"

namespace {

// Max visual tooltip width in DIPs. Beyond this, Cocoa will wrap text.
const int kTooltipMaxWidthPixels = 250;

}  // namespace

namespace views {

TooltipManagerMac::TooltipManagerMac(
    remote_cocoa::mojom::NativeWidgetNSWindow* bridge)
    : bridge_(bridge) {}

TooltipManagerMac::~TooltipManagerMac() {
}

int TooltipManagerMac::GetMaxWidth(const gfx::Point& location) const {
  return kTooltipMaxWidthPixels;
}

const gfx::FontList& TooltipManagerMac::GetFontList() const {
  static base::NoDestructor<gfx::FontList> font_list(
      []() { return gfx::Font([NSFont toolTipsFontOfSize:0]); }());
  return *font_list;
}

void TooltipManagerMac::UpdateTooltip() {
  bridge_->UpdateTooltip();
}

void TooltipManagerMac::TooltipTextChanged(View* view) {
  // The intensive part is View::GetTooltipHandlerForPoint(), which will be done
  // in [BridgedContentView updateTooltipIfRequiredAt:]. Don't do it here as
  // well.
  UpdateTooltip();
}

}  // namespace views
