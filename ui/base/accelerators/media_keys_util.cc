// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/media_keys_util.h"

#include "base/metrics/histogram_macros.h"

namespace ui {

const char kMediaHardwareKeyActionHistogramName[] = "Media.HardwareKeyPressed";

void RecordMediaHardwareKeyAction(ui::MediaHardwareKeyAction action) {
  UMA_HISTOGRAM_ENUMERATION(kMediaHardwareKeyActionHistogramName, action);
}

}  // namespace ui
