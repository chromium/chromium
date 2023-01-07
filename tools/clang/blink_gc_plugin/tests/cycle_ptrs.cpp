// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cycle_ptrs.h"

namespace blink {

void A::Trace(Visitor* visitor) const {
  visitor->Trace(m_b);
}

void B::Trace(Visitor* visitor) const {
  visitor->Trace(m_a);
}
}
