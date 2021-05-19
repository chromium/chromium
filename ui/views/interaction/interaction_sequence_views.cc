// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_sequence_views.h"

#include <utility>

#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

// static
std::unique_ptr<ui::InteractionSequence::Step>
InteractionSequenceViews::WithInitialView(
    View* view,
    ui::InteractionSequence::StepCallback start_callback,
    ui::InteractionSequence::StepCallback end_callback) {
  // If there's already an element associated with this view, then explicitly
  // key off of that element.
  auto* const element =
      ElementTrackerViews::GetInstance()->GetElementForView(view);
  if (element)
    return ui::InteractionSequence::WithInitialElement(
        element, std::move(start_callback), std::move(end_callback));

  // Otherwise, use the element's identifier and context.
  ui::ElementContext context = ElementTrackerViews::GetContextForView(view);
  ui::ElementIdentifier identifier = view->GetProperty(kElementIdentifierKey);
  return ui::InteractionSequence::StepBuilder()
      .SetContext(context)
      .SetElementID(identifier)
      .SetType(ui::InteractionSequence::StepType::kShown)
      .SetMustBeVisibleAtStart(true)
      .SetMustRemainVisible(true)
      .SetStartCallback(std::move(start_callback))
      .SetEndCallback(std::move(end_callback))
      .Build();
}

}  // namespace views
