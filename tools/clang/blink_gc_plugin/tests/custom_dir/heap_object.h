// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_DIR_HEAP_OBJECT_H_
#define TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_DIR_HEAP_OBJECT_H_

#include "heap/stubs.h"

// This class is not in a checked namespace, but lives in a checked directory.
// It should produce diagnostics when check-directory=custom_dir/ is passed.
namespace unchecked {

class HeapObject : public cppgc::GarbageCollected<HeapObject> {
  cppgc::Member<HeapObject> m_member;
};

}  // namespace unchecked

#endif  // TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_DIR_HEAP_OBJECT_H_
