// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTION_SEQUENCE_VIEWS_H_
#define UI_VIEWS_INTERACTION_INTERACTION_SEQUENCE_VIEWS_H_

#include <memory>

#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/views_export.h"

namespace views {

class View;

// Provides utility methods for using ui::InteractionsSequence with Views.
class VIEWS_EXPORT InteractionSequenceViews {
 public:
  // Not constructible.
  InteractionSequenceViews() = delete;

  static std::unique_ptr<ui::InteractionSequence::Step> WithInitialView(
      View* view,
      ui::InteractionSequence::StepCallback start_callback =
          ui::InteractionSequence::StepCallback(),
      ui::InteractionSequence::StepCallback end_callback =
          ui::InteractionSequence::StepCallback());
};

}  // namespace views

#endif  // UI_VIEWS_INTERACTION_INTERACTION_SEQUENCE_VIEWS_H_
