// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_DISPLAY_PROPERTIES_PARSER_H_
#define UI_DISPLAY_MANAGER_DISPLAY_PROPERTIES_PARSER_H_

#include "base/values.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace display {

DISPLAY_MANAGER_EXPORT
std::optional<gfx::RoundedCornersF> ParseDisplayPanelRadii(
    const base::Value* json_value);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_DISPLAY_PROPERTIES_PARSER_H_
