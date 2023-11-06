// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/events_features.h"

namespace features {

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kSwapBackquoteKeysInISOKeyboard,
             "SwapBackquoteKeysInISOKeyboard",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features
