// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_after_dispatch.h"

namespace blink {

static const B* toB(const A* a) {
  return static_cast<const B*>(a);
}

void A::Trace(Visitor* visitor) const {
  switch (m_type) {
    case TB:
      toB(this)->TraceAfterDispatch(visitor);
      break;
    case TC:
      static_cast<const C*>(this)->TraceAfterDispatch(visitor);
      break;
    case TD:
      // Missing static_cast<D*>(this)->TraceAfterDispatch(visitor);
      break;
  }
}

void A::TraceAfterDispatch(Visitor* visitor) const {}

void B::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(m_a);
  // Missing A::TraceAfterDispatch(visitor);
  // Also check that calling Trace does not count.
  A::Trace(visitor);
}

void C::TraceAfterDispatch(Visitor* visitor) const {
  // Missing visitor->Trace(m_a);
  A::TraceAfterDispatch(visitor);
}

void D::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(m_a);
  Abstract::TraceAfterDispatch(visitor);
}
}
