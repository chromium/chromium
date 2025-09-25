// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COLLECTION_OF_TRACEABLE_H_
#define COLLECTION_OF_TRACEABLE_H_

#include "heap/stubs.h"

namespace blink {

class Traceable {
  DISALLOW_NEW();

 public:
  virtual void Trace(Visitor*) const {}
};

}  // namespace blink

#endif  // COLLECTION_OF_TRACEABLE_H_
