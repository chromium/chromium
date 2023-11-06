// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENTS_FEATURES_H_
#define UI_EVENTS_EVENTS_FEATURES_H_

#include "base/feature_list.h"
#include "ui/events/events_base_export.h"

namespace features {

#if BUILDFLAG(IS_MAC)
// If enabled then the Backquote and IntlBackslash keys will be swapped when
// the user has an "ISO" keyboard configured (see crbug.com/600607).
// TODO(https://crbug.com/1296783): Remove this once we have confirmed that
// removing the swap does not break things for "ISO" keyboard users.
EVENTS_BASE_EXPORT BASE_DECLARE_FEATURE(kSwapBackquoteKeysInISOKeyboard);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features

#endif  // UI_EVENTS_EVENTS_FEATURES_H_
