// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_FORBIDDEN_FIELDS_H_
#define TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_FORBIDDEN_FIELDS_H_

#include "heap/stubs.h"

namespace blink {

template <typename T>
class TaskRunnerTimer {};

class SecondLevelPartObject {
 private:
  TaskRunnerTimer<SecondLevelPartObject> timer_;
};

class FirstLevelPartObject {
 private:
  SecondLevelPartObject obj_;
};

class HeapObject : public GarbageCollected<HeapObject> {
 private:
  FirstLevelPartObject obj_;
};

class AnotherHeapObject : public GarbageCollected<AnotherHeapObject> {
 private:
  TaskRunnerTimer<AnotherHeapObject> timer_;
};

}  // namespace blink

#endif /* TOOLS_CLANG_BLINK_GC_PLUGIN_TESTS_FORBIDDEN_FIELDS_H_ */
