// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_MULTIPLE_NAMESPACES_H_
#define TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_MULTIPLE_NAMESPACES_H_

#include "heap/stubs.h"

namespace custom_a {

class HeapObjectA : public cppgc::GarbageCollected<HeapObjectA> {
  cppgc::Member<HeapObjectA> m_member;
};

}  // namespace custom_a

namespace custom_b {

class HeapObjectB : public cppgc::GarbageCollected<HeapObjectB> {
  cppgc::Member<HeapObjectB> m_member;
};

}  // namespace custom_b

// This namespace is not added, so it should not be checked.
namespace unchecked {

class HeapObjectUnchecked
    : public cppgc::GarbageCollected<HeapObjectUnchecked> {
  cppgc::Member<HeapObjectUnchecked> m_member;
};

}  // namespace unchecked

#endif  // TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_CUSTOM_MULTIPLE_NAMESPACES_H_
