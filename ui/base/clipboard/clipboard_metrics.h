// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_METRICS_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_METRICS_H_

#include "build/build_config.h"

namespace ui {

#if BUILDFLAG(IS_CHROMEOS)
class ClipboardData;
#endif  // BUILDFLAG(IS_CHROMEOS)

// Used to log formats read/written from/to the platform clipboard.
//
// This enum's values are persisted to logs. Do not reuse or renumber values.
enum class ClipboardFormatMetric {
  kText = 0,  // On applicable platforms, includes both UNICODE and ANSI text.
  kHtml = 1,
  kRtf = 2,
  kImage = 3,  // Bitmap data.
  kBookmark = 4,
  kData = 5,
  kCustomData = 6,
  kWebSmartPaste = 7,  // Only used on write.
  kSvg = 8,
  kFilenames = 9,
  kPng = 10,
  kMaxValue = kPng,
};

void RecordRead(ClipboardFormatMetric metric);
void RecordWrite(ClipboardFormatMetric metric);

#if BUILDFLAG(IS_CHROMEOS)
// Records the time interval between when the specified |data| was committed to
// the clipboard and when it was read. Read time is assumed to be now.
void RecordTimeIntervalBetweenCommitAndRead(const ui::ClipboardData* data);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_METRICS_H_
