// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_after_dispatch_impl_error.h"

namespace blink {

inline void TraceAfterDispatchInlinedBase::Trace(Visitor* visitor) const {
  // Implement a simple form of manual dispatching, because BlinkGCPlugin
  // checks if the tracing is dispatched to all derived classes.
  //
  // This function has to be implemented out-of-line, since we need to know the
  // definition of derived classes here.
  if (tag_ == DERIVED) {
    // Missing dispatch call:
    // static_cast<TraceAfterDispatchInlinedDerived*>(this)->TraceAfterDispatch(
    //     visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

void TraceAfterDispatchExternBase::Trace(Visitor* visitor) const {
  if (tag_ == DERIVED) {
    // Missing dispatch call:
    // static_cast<TraceAfterDispatchExternDerived*>(this)->TraceAfterDispatch(
    //     visitor);
  } else {
    TraceAfterDispatch(visitor);
  }
}

void TraceAfterDispatchExternBase::TraceAfterDispatch(Visitor* visitor) const {
  // No Trace call.
}

void TraceAfterDispatchExternDerived::TraceAfterDispatch(
    Visitor* visitor) const {
  // Ditto.
}
}
