// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/clipboard/clipboard_data.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace ui {

void RecordRead(ClipboardFormatMetric metric) {
  base::UmaHistogramEnumeration("Clipboard.Read", metric);
}

void RecordWrite(ClipboardFormatMetric metric) {
  base::UmaHistogramEnumeration("Clipboard.Write", metric);
}

#if BUILDFLAG(IS_CHROMEOS)
void RecordTimeIntervalBetweenCommitAndRead(const ui::ClipboardData* data) {
  if (!data)
    return;

  std::optional<base::Time> commit_time = data->commit_time();
  if (!commit_time.has_value())
    return;

  base::UmaHistogramCustomTimes("Clipboard.TimeIntervalBetweenCommitAndRead",
                                base::Time::Now() - commit_time.value(),
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Hours(12), /*buckets=*/100);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace ui
