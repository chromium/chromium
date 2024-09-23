// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/input_method_ukm.h"

#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/ui_base_features.h"

namespace ash {

void RecordUkmNonCompliantApi(
    ukm::SourceId source,
    const ime::mojom::InputMethodApiOperation operation) {
  if (source == ukm::kInvalidSourceId ||
      operation == ime::mojom::InputMethodApiOperation::kUnknown) {
    return;
  }

  // After this metric was added, a default value of zero was added to
  // ash.ime.mojom.InputMethodApiOperation, which shifted all the values by
  // one. So subtract one to ensure the metric is still correct.
  ukm::builders::InputMethod_NonCompliantApi(source)
      .SetNonCompliantOperation(static_cast<int>(operation) - 1)
      .Record(ukm::UkmRecorder::Get());
}

void RecordUkmAssistiveMatch(ukm::SourceId source, const int64_t type) {
  if (source == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::InputMethod_Assistive_Match(source).SetType(type).Record(
      ukm::UkmRecorder::Get());
}

}  // namespace ash
