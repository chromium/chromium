// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IGNORE_FILE_H_
#define IGNORE_FILE_H_

#include "heap/stubs.h"

GC_PLUGIN_IGNORE_FILE("http://crbug.com/12345")

namespace blink {

class HeapObject : public GarbageCollected<HeapObject> {};

// Don't require Trace method on ignored class.
class IgnoredClass : public GarbageCollected<IgnoredClass> {
 private:
  Member<HeapObject> m_obj;
};

}  // namespace blink

#endif  // IGNORE_FILE_H_
