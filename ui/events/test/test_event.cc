// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_event.h"

namespace ui::test {

std::unique_ptr<Event> TestEvent::Clone() const {
  return std::make_unique<TestEvent>(*this);
}

}  // namespace ui::test
