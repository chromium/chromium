// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garbage_collected_mixin.h"

namespace blink {

void Mixin::Trace(Visitor* visitor) const {
  // Missing: visitor->Trace(m_self);
}

void HeapObject::Trace(Visitor* visitor) const {
  visitor->Trace(m_mix);
  // Missing: Mixin::Trace(visitor);
}
}
