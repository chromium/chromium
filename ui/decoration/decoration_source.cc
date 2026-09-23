// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/decoration/decoration_source.h"

namespace ui::decoration {

DecorationSource::DecorationSource() = default;

DecorationSource::~DecorationSource() = default;

void DecorationSource::NotifyDecorationChanged(
    std::optional<base::TimeDelta> cross_fade_duration) {
  if (on_details_changed_callback_) {
    on_details_changed_callback_.Run(cross_fade_duration);
  }
}

}  // namespace ui::decoration
