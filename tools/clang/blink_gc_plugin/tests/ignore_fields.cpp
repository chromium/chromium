// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ignore_fields.h"

namespace blink {

void C::Trace(Visitor* visitor) const {
  // Missing Trace of m_one.
  // Not missing ignored field m_two.
}
}
