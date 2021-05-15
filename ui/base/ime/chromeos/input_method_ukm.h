// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_INPUT_METHOD_UKM_H_
#define UI_BASE_IME_CHROMEOS_INPUT_METHOD_UKM_H_

#include "base/component_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/ime/text_input_type.h"

namespace ui {

// Records an event in UKM, under the InputMethod.NonCompliantApi metric.
// Ignores invalid sources.
// `operation` is a value in the chromeos.ime.mojom.InputMethodApiOperation
// enum.
COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS)
void RecordUkmNonCompliantApi(ukm::SourceId source, int64_t operation);

}  // namespace ui

#endif  // UI_BASE_IME_CHROMEOS_INPUT_METHOD_UKM_H_
