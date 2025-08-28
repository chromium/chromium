// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/pointer_device.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "build/build_config.h"

namespace ui {

// Platform-specific files implement this.
std::pair<int, int> GetAvailablePointerAndHoverTypesImpl();

namespace {

ScopedSetPointerAndHoverTypesForTesting* g_pointer_and_hover_types_for_testing =
    nullptr;

}

ScopedSetPointerAndHoverTypesForTesting::
    ScopedSetPointerAndHoverTypesForTesting(int available_pointer_types,
                                            int available_hover_types)
    : pointer_and_hover_types_(
          {available_pointer_types, available_hover_types}) {
  // Currently no need for nested scopers.
  CHECK(!g_pointer_and_hover_types_for_testing);
  g_pointer_and_hover_types_for_testing = this;
}

ScopedSetPointerAndHoverTypesForTesting::
    ~ScopedSetPointerAndHoverTypesForTesting() {
  g_pointer_and_hover_types_for_testing = nullptr;
}

std::pair<int, int> GetAvailablePointerAndHoverTypes() {
  return g_pointer_and_hover_types_for_testing
             ? g_pointer_and_hover_types_for_testing->pointer_and_hover_types()
             : GetAvailablePointerAndHoverTypesImpl();
}

#if !BUILDFLAG(IS_ANDROID)
PointerType GetPrimaryPointerType() {
  const int available_pointer_types = GetAvailablePointerAndHoverTypes().first;
  if (available_pointer_types & POINTER_TYPE_FINE) {
    return POINTER_TYPE_FINE;
  }
  if (available_pointer_types & POINTER_TYPE_COARSE) {
    return POINTER_TYPE_COARSE;
  }
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

HoverType GetPrimaryHoverType() {
  const int available_hover_types = GetAvailablePointerAndHoverTypes().second;
  if (available_hover_types & HOVER_TYPE_HOVER) {
    return HOVER_TYPE_HOVER;
  }
  DCHECK_EQ(available_hover_types, HOVER_TYPE_NONE);
  return HOVER_TYPE_NONE;
}
#endif

#if !BUILDFLAG(IS_WIN)
std::optional<PointerDevice> GetPointerDevice(PointerDevice::Key key) {
  return std::nullopt;
}

std::vector<PointerDevice> GetPointerDevices() {
  return {};
}
#endif

}  // namespace ui
