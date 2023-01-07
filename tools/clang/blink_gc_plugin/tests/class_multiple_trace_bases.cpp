// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "class_multiple_trace_bases.h"

namespace blink {

void Base::Trace(Visitor* visitor) const {}

void Mixin1::Trace(Visitor* visitor) const {}

void Mixin2::Trace(Visitor* visitor) const {}

// Missing: void Derived1::Trace(Visitor* visitor) const;

void Derived2::Trace(Visitor* visitor) const {
  Base::Trace(visitor);
  Mixin1::Trace(visitor);
}
}
