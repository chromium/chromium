// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_if_needed_resolved.h"

namespace blink {

void HeapObject::Trace(Visitor* visitor) const {
  // Using TraceIfNeeded with a non-template type should count as tracing a
  // field.
  TraceIfNeeded<Member<HeapObject>>::Trace(visitor, m_one);
  TraceIfNeeded<int>::Trace(visitor, m_two);
}

}  // namespace blink
