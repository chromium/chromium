// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cycle_sub.h"

namespace blink {

void B::Trace(Visitor* visitor) const {
  visitor->Trace(m_c);
  A::Trace(visitor);
}
}
