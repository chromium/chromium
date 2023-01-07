// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_MAC_SYSTEM_COLOR_UTILS_H_
#define UI_COLOR_MAC_SYSTEM_COLOR_UTILS_H_

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ui {

COMPONENT_EXPORT(COLOR) bool IsSystemGraphiteTinted();

COMPONENT_EXPORT(COLOR) SkColor ColorToGrayscale(SkColor color);

class COMPONENT_EXPORT(COLOR) ScopedEnableGraphiteTint {
 public:
  ScopedEnableGraphiteTint();
  ScopedEnableGraphiteTint(const ScopedEnableGraphiteTint&) = delete;
  ScopedEnableGraphiteTint& operator=(const ScopedEnableGraphiteTint&) = delete;
  ~ScopedEnableGraphiteTint();

 private:
  bool original_test_override_ = false;
};

}  // namespace ui

#endif  // UI_COLOR_MAC_SYSTEM_COLOR_UTILS_H_
