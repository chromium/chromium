// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "class_requires_trace_method_tmpl.h"

namespace blink {

// Does not need a Trace method.
class NoTrace : public TemplatedObject<PartObjectA> { };

// Needs a Trace method.
class NeedsTrace : public TemplatedObject<PartObjectB> { };

}
