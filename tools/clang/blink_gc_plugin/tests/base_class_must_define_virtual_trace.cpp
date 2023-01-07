// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base_class_must_define_virtual_trace.h"

namespace blink {

void PartDerived::Trace(Visitor* visitor) const {}

void HeapDerived::Trace(Visitor* visitor) const {
  visitor->Trace(m_part);
}
}
