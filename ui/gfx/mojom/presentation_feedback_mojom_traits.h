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

  static base::TimeTicks available_timestamp(
      const gfx::PresentationFeedback& input) {
    return input.available_timestamp;
  }

  static base::TimeTicks ready_timestamp(
      const gfx::PresentationFeedback& input) {
    return input.ready_timestamp;
  }

  static base::TimeTicks latch_timestamp(
      const gfx::PresentationFeedback& input) {
    return input.latch_timestamp;
  }

  static base::TimeTicks writes_done_timestamp(
      const gfx::PresentationFeedback& input) {
    return input.writes_done_timestamp;
  }

  static bool Read(gfx::mojom::PresentationFeedbackDataView data,
                   gfx::PresentationFeedback* out) {
    out->flags = data.flags();
    return data.ReadTimestamp(&out->timestamp) &&
           data.ReadInterval(&out->interval) &&
           data.ReadAvailableTimestamp(&out->available_timestamp) &&
           data.ReadReadyTimestamp(&out->ready_timestamp) &&
           data.ReadLatchTimestamp(&out->latch_timestamp) &&
           data.ReadWritesDoneTimestamp(&out->writes_done_timestamp);
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_PRESENTATION_FEEDBACK_MOJOM_TRAITS_H_
