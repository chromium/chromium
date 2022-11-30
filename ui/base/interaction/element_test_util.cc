// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/element_test_util.h"

#include "base/test/bind.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui::test {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestFrameworkIdentifier);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherFrameworkIdentifier);

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

void TestElementBase::SendCustomEvent(CustomElementEventType event_type) {
  DCHECK(visible_);
  ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(this, event_type);
}

// static
TrackedElement::FrameworkIdentifier TestElement::GetFrameworkIdentifier() {
  return kTestFrameworkIdentifier;
}

TrackedElement::FrameworkIdentifier
TestElement::GetInstanceFrameworkIdentifier() const {
  return kTestFrameworkIdentifier;
}

// static
TrackedElement::FrameworkIdentifier
TestElementOtherFramework::GetFrameworkIdentifier() {
  return kOtherFrameworkIdentifier;
}

TrackedElement::FrameworkIdentifier
TestElementOtherFramework::GetInstanceFrameworkIdentifier() const {
  return kOtherFrameworkIdentifier;
}

}  // namespace ui::test
