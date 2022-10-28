// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_METRICS_H_
#define UI_COLOR_COLOR_METRICS_H_

#include "base/component_export.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ui {

void COMPONENT_EXPORT(COLOR) RecordColorProviderCacheSize(int cache_size);
void COMPONENT_EXPORT(COLOR)
    RecordNumColorProvidersInitializedDuringOnNativeThemeUpdated(
        int num_providers);
void COMPONENT_EXPORT(COLOR)
    RecordTimeSpentInitializingColorProvider(base::TimeDelta duration);
void COMPONENT_EXPORT(COLOR) RecordTimeSpentProcessingOnNativeThemeUpdatedEvent(
    base::TimeDelta duration);

}  // namespace ui

#endif  // UI_COLOR_COLOR_METRICS_H_
