// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "non_virtual_trace.h"

namespace blink {

void A::Trace(Visitor* visitor) const {}

void C::Trace(Visitor* visitor) const {
  B::Trace(visitor);
}

void D::Trace(Visitor* visitor) const {
  B::Trace(visitor);
}
}
