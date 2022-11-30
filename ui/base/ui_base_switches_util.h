// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_SWITCHES_UTIL_H_
#define UI_BASE_UI_BASE_SWITCHES_UTIL_H_

#include "base/component_export.h"

namespace switches {

COMPONENT_EXPORT(UI_BASE) bool IsElasticOverscrollEnabled();
COMPONENT_EXPORT(UI_BASE) bool IsTouchDragDropEnabled();

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_UTIL_H_
