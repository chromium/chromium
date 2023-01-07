// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "polymorphic_class_with_non_virtual_trace.h"

namespace blink {

void IsLeftMostPolymorphic::Trace(Visitor* visitor) const {
  visitor->Trace(m_obj);
}

void IsNotLeftMostPolymorphic::Trace(Visitor* visitor) const {
  visitor->Trace(m_obj);
}
}
