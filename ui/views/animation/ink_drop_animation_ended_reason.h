// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_ENDED_REASON_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_ENDED_REASON_H_

#include <iosfwd>
#include <string>

#include "ui/views/views_export.h"

namespace views {

// Enumeration of the different reasons why an ink drop animation has finished.
enum class InkDropAnimationEndedReason {
  // The animation was completed successfully.
  SUCCESS,
  // The animation was stopped prematurely before reaching its final state.
  PRE_EMPTED
};

// Returns a human readable string for |reason|.  Useful for logging.
VIEWS_EXPORT std::string ToString(InkDropAnimationEndedReason reason);

// This is declared here for use in gtest-based unit tests but is defined in
// the views_test_support target. Depend on that to use this in your unit test.
// This should not be used in production code - call ToString() instead.
void PrintTo(InkDropAnimationEndedReason reason, ::std::ostream* os);

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_ENDED_REASON_H_
