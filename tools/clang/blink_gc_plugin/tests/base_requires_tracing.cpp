// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_requires_tracing.h"

namespace blink {

void A::Trace(Visitor* visitor) const {}

void C::Trace(Visitor* visitor) const {
  visitor->Trace(m_a);
  // Missing B::trace(visitor)
}

void D::Trace(Visitor* visitor) const {
  visitor->Trace(m_a);
  C::Trace(visitor);
}
}
