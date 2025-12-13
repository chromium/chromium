// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/button_test_api.h"

#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"

namespace views::test {

void ButtonTestApi::NotifyClick(const ui::Event& event) {
  button_->NotifyClick(event);
}

void ButtonTestApi::NotifyDefaultMouseClick() {
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(), ui::EF_NONE,
                             ui::EF_NONE);
  button_->NotifyClick(event);
}

}  // namespace views::test
