// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "traceimpl_overloaded.h"

namespace blink {

void ExternBase::Trace(Visitor* visitor) const {
  visitor->Trace(x_base_);
}

void ExternDerived::Trace(Visitor* visitor) const {
  visitor->Trace(x_derived_);
  ExternBase::Trace(visitor);
}
}
