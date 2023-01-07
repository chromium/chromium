// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "virtual_and_trace_after_dispatch.h"

namespace blink {

static const B* toB(const A* a) {
  return static_cast<const B*>(a);
}

void A::Trace(Visitor* visitor) const {
  switch (m_type) {
    case TB:
        toB(this)->TraceAfterDispatch(visitor);
        break;
  }
}

void A::TraceAfterDispatch(Visitor* visitor) const {}

void B::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(m_a);
  A::TraceAfterDispatch(visitor);
}
}
