// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "traceimpl_dependent_scope.h"

namespace blink {

struct Empty {};

// Template instantiation.
template class Derived<int>;
template class DerivedMissingTrace<int>;
template class Mixin<X>;
template class MixinMissingTrace<X>;
template class MixinTwoBases<X, Y>;
template class MixinTwoBasesMissingTrace<X, Y>;
template class MixinTwoBasesMissingTrace<X, Empty>;  // This should be fine.
}
