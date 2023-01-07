// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cycle_super.h"

namespace blink {

void A::Trace(Visitor* visitor) const {
  visitor->Trace(m_d);
}

void B::Trace(Visitor* visitor) const {
  A::Trace(visitor);
}

void C::Trace(Visitor* visitor) const {
  B::Trace(visitor);
}
}
