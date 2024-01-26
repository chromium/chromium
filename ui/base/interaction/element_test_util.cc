// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_test_util.h"

#include "base/test/bind.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace ui::test {

TestElementBase::TestElementBase(ElementIdentifier id, ElementContext context)
    : TrackedElement(id, context) {}

TestElementBase::~TestElementBase() {
  Hide();
}

TestElement::TestElement(ElementIdentifier id, ElementContext context)
    : TestElementBase(id, context) {}

TestElementOtherFramework::TestElementOtherFramework(ElementIdentifier id,
                                                     ElementContext context)
    : TestElementBase(id, context) {}

void TestElementBase::Show() {
  if (visible_)
    return;
  visible_ = true;
  ElementTracker::GetFrameworkDelegate()->NotifyElementShown(this);
}

void TestElementBase::Activate() {
  DCHECK(visible_);
  ElementTracker::GetFrameworkDelegate()->NotifyElementActivated(this);
}

void TestElementBase::Hide() {
  if (!visible_)
    return;
  visible_ = false;
  ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(this);
}

bool TestElementBase::IsVisible() const {
  return visible_;
}

void TestElementBase::SendCustomEvent(CustomElementEventType event_type) {
  DCHECK(visible_);
  ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(this, event_type);
}

void TestElementBase::SetScreenBounds(const gfx::Rect& screen_bounds) {
  screen_bounds_ = screen_bounds;
}

gfx::Rect TestElementBase::GetScreenBounds() const {
  return screen_bounds_;
}

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestElement)
DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestElementOtherFramework)

}  // namespace ui::test
