// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "member_in_offheap_class.h"

namespace blink {

void OffHeapObject::Trace(Visitor* visitor) const {
  visitor->Trace(m_obj);
  visitor->Trace(m_weak);
}

void PartObject::Trace(Visitor* visitor) const {
  visitor->Trace(m_obj);
}

void DerivedPartObject::Trace(Visitor* visitor) const {
  visitor->Trace(m_obj1);
  PartObject::Trace(visitor);
}
}
