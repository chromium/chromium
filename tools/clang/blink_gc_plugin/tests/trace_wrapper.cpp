// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_wrapper.h"

namespace blink {

void B::Trace(Visitor* visitor) const {
  static_cast<const C*>(this)->TraceAfterDispatch(visitor);
}

}  // namespace blink
