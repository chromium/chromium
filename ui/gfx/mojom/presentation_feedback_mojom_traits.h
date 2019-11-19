// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_PRESENTATION_FEEDBACK_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_PRESENTATION_FEEDBACK_MOJOM_TRAITS_H_

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/gfx/mojom/presentation_feedback.mojom-shared.h"
#include "ui/gfx/presentation_feedback.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::PresentationFeedbackDataView,
                    gfx::PresentationFeedback> {
  static base::TimeTicks timestamp(const gfx::PresentationFeedback& input) {
    return input.timestamp;
  }

  static base::TimeDelta interval(const gfx::PresentationFeedback& input) {
    return input.interval;
  }

  static uint32_t flags(const gfx::PresentationFeedback& input) {
    return input.flags;
  }

  static bool Read(gfx::mojom::PresentationFeedbackDataView data,
                   gfx::PresentationFeedback* out) {
    out->flags = data.flags();
    return data.ReadTimestamp(&out->timestamp) &&
           data.ReadInterval(&out->interval);
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_PRESENTATION_FEEDBACK_MOJOM_TRAITS_H_
