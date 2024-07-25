// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_UTIL_DISPLAY_MANAGER_TEST_UTIL_H_
#define UI_DISPLAY_MANAGER_UTIL_DISPLAY_MANAGER_TEST_UTIL_H_

#include <stdint.h>

#include "ui/display/manager/display_manager_export.h"

namespace display {

// Resets the synthesized display id for testing. This
// is necessary to avoid overflowing the output index.
DISPLAY_MANAGER_EXPORT void ResetDisplayIdForTest();

// Generates a fake, synthesized display ID..
DISPLAY_MANAGER_EXPORT int64_t GetASynthesizedDisplayId();

// Uses |id| as a seed to produce the next synthesized ID.
DISPLAY_MANAGER_EXPORT int64_t SynthesizeDisplayIdFromSeed(int64_t id);

// Generates the next fake connector index for displays who's ID was generated
// by hashing their EDIDs.
DISPLAY_MANAGER_EXPORT int64_t GetNextSynthesizedEdidDisplayConnectorIndex();

// Increases the most significant digit of |id| by one. For example:
// for 2200000000, this function returns 3200000000.
// Note: this method is chosen for readability purposes only. It creates a an
// easy-to-read relationship between |id| and the returned value, so that a
// reader of test logs can look at the two IDs and recognize they are related.
DISPLAY_MANAGER_EXPORT int64_t ProduceAlternativeSchemeIdForId(int64_t id);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_UTIL_DISPLAY_MANAGER_TEST_UTIL_H_
