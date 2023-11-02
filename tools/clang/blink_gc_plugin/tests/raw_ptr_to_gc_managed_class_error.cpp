// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "raw_ptr_to_gc_managed_class_error.h"

namespace blink {

void HeapObject::Trace(Visitor* visitor) const {
  visitor->Trace(m_objs);
}
}
