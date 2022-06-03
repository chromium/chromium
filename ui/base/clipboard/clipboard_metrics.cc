// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace ui {

void RecordRead(ClipboardFormatMetric metric) {
  base::UmaHistogramEnumeration("Clipboard.Read", metric);
}

void RecordWrite(ClipboardFormatMetric metric) {
  base::UmaHistogramEnumeration("Clipboard.Write", metric);
}

}  // namespace ui
