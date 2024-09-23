// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/interaction_sequence_views.h"

#include <string_view>
#include <utility>

#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

// static
std::unique_ptr<ui::InteractionSequence::Step>
InteractionSequenceViews::WithInitialView(
    View* view,
    ui::InteractionSequence::StepStartCallback start_callback,
    ui::InteractionSequence::StepEndCallback end_callback) {
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

// static
void InteractionSequenceViews::NameView(ui::InteractionSequence* sequence,
                                        View* view,
                                        std::string_view name) {
  ui::TrackedElement* element = nullptr;
  if (view) {
    element = ElementTrackerViews::GetInstance()->GetElementForView(
        view, /* assign_temporary_id =*/true);
    DCHECK(element);
  }
  sequence->NameElement(element, name);
}

}  // namespace views
