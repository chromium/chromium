// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/features.h"

#include "build/build_config.h"

namespace ui {

BASE_FEATURE(kLegacyKeyRepeatSynthesis,
             "LegacyKeyRepeatSynthesis",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace ui
