// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_INPUT_METHOD_UKM_H_
#define UI_BASE_IME_ASH_INPUT_METHOD_UKM_H_

#include "base/component_export.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom-shared.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/ime/text_input_type.h"

namespace ash {

// Records an event in UKM, under the InputMethod.NonCompliantApi metric.
// Ignores invalid sources.
COMPONENT_EXPORT(UI_BASE_IME_ASH)
void RecordUkmNonCompliantApi(ukm::SourceId source,
                              ime::mojom::InputMethodApiOperation operation);

// Records an event in UKM, under the InputMethod.Assistive.Match metric.
// Ignores invalid sources.
// `type` is a value in the chromeos.AssistiveType enum.
COMPONENT_EXPORT(UI_BASE_IME_ASH)
void RecordUkmAssistiveMatch(ukm::SourceId source, int64_t type);

}  // namespace ash

#endif  // UI_BASE_IME_ASH_INPUT_METHOD_UKM_H_
