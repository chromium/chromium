// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_window_targeter.h"

namespace aura {
namespace test {

TestWindowTargeter::TestWindowTargeter() {}

TestWindowTargeter::~TestWindowTargeter() {}

ui::EventTarget* TestWindowTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                        ui::Event* event) {
  return root;
}

ui::EventTarget* TestWindowTargeter::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  return previous_target;
}

}  // namespace test
}  // namespace aura
