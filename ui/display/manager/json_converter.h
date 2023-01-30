// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_JSON_CONVERTER_H_
#define UI_DISPLAY_MANAGER_JSON_CONVERTER_H_

#include "base/values.h"
#include "ui/display/manager/display_manager_export.h"

namespace display {

class DisplayLayout;

DISPLAY_MANAGER_EXPORT bool JsonToDisplayLayout(const base::Value::Dict& dict,
                                                DisplayLayout* layout);

// This will return false if `value` is not a dict.
// Otherwise this will call the overload above.
DISPLAY_MANAGER_EXPORT bool JsonToDisplayLayout(const base::Value& value,
                                                DisplayLayout* layout);

// This will modify `dict` in place.
DISPLAY_MANAGER_EXPORT void DisplayLayoutToJson(const DisplayLayout& layout,
                                                base::Value::Dict& dict);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_JSON_CONVERTER_H_
