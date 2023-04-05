// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TYPES_SCROLL_TYPES_H_
#define UI_EVENTS_TYPES_SCROLL_TYPES_H_

#include <stdint.h>

namespace ui {

enum class ScrollGranularity : uint8_t {
  kScrollByPrecisePixel = 0,
  kFirstScrollGranularity = kScrollByPrecisePixel,
  kScrollByPixel,
  kScrollByLine,
  kScrollByPage,
  kScrollByDocument,
  kScrollByPercentage,
  kMaxValue = kScrollByPercentage
};

}  // namespace ui

#endif  // UI_EVENTS_TYPES_SCROLL_TYPES_H_
