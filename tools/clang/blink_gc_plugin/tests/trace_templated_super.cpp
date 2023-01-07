// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "trace_templated_super.h"

namespace blink {

template<typename T>
void Super<T>::clearWeakMembers(Visitor* visitor)
{
    (void)m_weak;
}

template <typename T>
void Super<T>::Trace(Visitor* visitor) const {
  visitor->RegisterWeakMembers<Super<T>, &Super<T>::clearWeakMembers>(this);
  visitor->Trace(m_obj);
  Mixin::Trace(visitor);
}

template <typename T>
void Sub<T>::Trace(Visitor* visitor) const {
  // Missing Trace of m_obj.
  Super<T>::Trace(visitor);
}

void HeapObject::Trace(Visitor* visitor) const {
  visitor->Trace(m_obj);
  Sub<HeapObject>::Trace(visitor);
}
}
