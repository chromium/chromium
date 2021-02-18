// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_METRICS_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_METRICS_H_

namespace ui {

// Used to log formats read/written from/to the platform clipboard.
//
// This enum's values are persisted to logs. Do not reuse or renumber values.
enum class ClipboardFormatMetric {
  kText = 0,  // On applicable platforms, includes both UNICODE and ANSI text.
  kHtml = 1,
  kRtf = 2,
  kImage = 3,
  kBookmark = 4,
  kData = 5,
  kCustomData = 6,
  kWebSmartPaste = 7,  // Only used on write.
  kSvg = 8,
  kFilenames = 9,
  kMaxValue = kFilenames,
};

void RecordRead(ClipboardFormatMetric metric);
void RecordWrite(ClipboardFormatMetric metric);

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_MONITOR_H_
