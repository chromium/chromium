// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "persistent_no_trace.h"

namespace blink {

void Object::Trace(Visitor* visitor) const {
  visitor->Trace(m_persistent);
  visitor->Trace(m_weakPersistent);
  visitor->Trace(m_crossThreadPersistent);
  visitor->Trace(m_crossThreadWeakPersistent);
}
}
