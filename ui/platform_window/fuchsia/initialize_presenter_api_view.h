// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_FUCHSIA_INITIALIZE_PRESENTER_API_VIEW_H_
#define UI_PLATFORM_WINDOW_FUCHSIA_INITIALIZE_PRESENTER_API_VIEW_H_

#include "base/component_export.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {
namespace fuchsia {

// Generates and sets the view tokens that are required to utilize the
// Presenter API. |window_properties_out| must be a valid value.
COMPONENT_EXPORT(PLATFORM_WINDOW)
void InitializeViewTokenAndPresentView(
    ui::PlatformWindowInitProperties* window_properties_out);

}  // namespace fuchsia
}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_FUCHSIA_INITIALIZE_PRESENTER_API_VIEW_H_
