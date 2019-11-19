// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_STYLED_LABEL_LISTENER_H_
#define UI_VIEWS_CONTROLS_STYLED_LABEL_LISTENER_H_

#include "ui/views/views_export.h"

namespace gfx {
class Range;
}

namespace views {

class StyledLabel;

// A listener interface for StyledLabel.
class VIEWS_EXPORT StyledLabelListener {
 public:
  virtual void StyledLabelLinkClicked(StyledLabel* label,
                                      const gfx::Range& range,
                                      int event_flags) = 0;

 protected:
  virtual ~StyledLabelListener() = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_STYLED_LABEL_LISTENER_H_
