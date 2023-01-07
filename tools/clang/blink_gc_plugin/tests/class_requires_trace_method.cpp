// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "class_requires_trace_method.h"

namespace blink {

void Mixin2::Trace(Visitor* visitor) const {
  Mixin::Trace(visitor);
}

void Mixin3::Trace(Visitor* visitor) const {
  Mixin::Trace(visitor);
}

} // namespace blink
