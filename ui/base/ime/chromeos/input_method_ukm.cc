// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/input_method_ukm.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/ui_base_features.h"

namespace ui {

void RecordUkmNonCompliantApi(ukm::SourceId source, const int64_t operation) {
  if (source == ukm::kInvalidSourceId)
    return;

  ukm::builders::InputMethod_NonCompliantApi(source)
      .SetNonCompliantOperation(operation)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace ui
