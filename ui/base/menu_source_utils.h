// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MENU_SOURCE_UTILS_H_
#define UI_BASE_MENU_SOURCE_UTILS_H_

#include "base/component_export.h"
#include "ui/base/ui_base_types.h"

namespace ui {

class Event;

COMPONENT_EXPORT(UI_BASE)
MenuSourceType GetMenuSourceTypeForEvent(const Event& event);

}  // namespace ui

#endif  // UI_BASE_MENU_SOURCE_UTILS_H_
