// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_AUTOFILL_ASSISTANT_WEBLAYER_ASSISTANT_FIELD_TRIAL_UTIL_H_
#define WEBLAYER_BROWSER_AUTOFILL_ASSISTANT_WEBLAYER_ASSISTANT_FIELD_TRIAL_UTIL_H_

#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"

namespace weblayer {

// Provides field trial utils for WebLayer.
class WebLayerAssistantFieldTrialUtil
    : public ::autofill_assistant::AssistantFieldTrialUtil {
  bool RegisterSyntheticFieldTrial(base::StringPiece trial_name,
                                   base::StringPiece group_name) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_AUTOFILL_ASSISTANT_WEBLAYER_ASSISTANT_FIELD_TRIAL_UTIL_H_
