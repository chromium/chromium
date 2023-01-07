// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MEDIA_KEYS_UTIL_H_
#define UI_BASE_ACCELERATORS_MEDIA_KEYS_UTIL_H_

#include "base/component_export.h"

namespace ui {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MediaHardwareKeyAction {
  kPlay = 0,
  kPause,
  kStop,
  kNextTrack,
  kPreviousTrack,
  kPlayPause,
  kSeekForward,
  kSeekBackward,
  kMaxValue = kSeekBackward
};

// The name of the histogram that records |MediaHardwareKeyAction|.
COMPONENT_EXPORT(UI_BASE)
extern const char kMediaHardwareKeyActionHistogramName[];

// Records a media hardware key action to the
// |kMediaHardwareKeyActionHistogramName| histogram.
COMPONENT_EXPORT(UI_BASE)
void RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction action);

}  // namespace ui

#endif  // UI_BASE_ACCELERATORS_MEDIA_KEYS_UTIL_H_
