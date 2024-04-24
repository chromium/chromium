// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTION_SEQUENCE_VIEWS_H_
#define UI_VIEWS_INTERACTION_INTERACTION_SEQUENCE_VIEWS_H_

#include <memory>
#include <string_view>

#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Provides utility methods for using ui::InteractionsSequence with Views.
class VIEWS_EXPORT InteractionSequenceViews {
 public:
  // Not constructible.
  InteractionSequenceViews() = delete;

  // Returns an InteractionSequence initial step with the specified `view`.
  static std::unique_ptr<ui::InteractionSequence::Step> WithInitialView(
      View* view,
      ui::InteractionSequence::StepStartCallback start_callback =
          ui::InteractionSequence::StepStartCallback(),
      ui::InteractionSequence::StepEndCallback end_callback =
          ui::InteractionSequence::StepEndCallback());

  // Given an InteractionSequence and a View, names the view in the sequence.
  // If the view doesn't already have an element identifier, assigns
  // ui::ElementTracker::kTemporaryIdentifier. If `view` is null, calls
  // sequence->NameElement(nullptr, name).
  //
  // It is an error to call this method on a non-null View which is not visible
  // or which is not attached to a Widget.
  static void NameView(ui::InteractionSequence* sequence,
                       View* view,
                       std::string_view name);
};

}  // namespace views

#endif  // UI_VIEWS_INTERACTION_INTERACTION_SEQUENCE_VIEWS_H_
