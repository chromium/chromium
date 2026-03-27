// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_NAMESPACE_H_
#define TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_NAMESPACE_H_

#include "heap/stubs.h"

namespace custom {

// This class is in the 'custom' namespace, which is not checked by default.
// It should only produce diagnostics when check-namespace=custom is
// passed as a plugin argument.
class HeapObject : public cppgc::GarbageCollected<HeapObject> {
  cppgc::Member<HeapObject> m_member;
};

}  // namespace custom

#endif  // TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_NAMESPACE_H_
