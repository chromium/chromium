// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRACE_WRAPPER_H_
#define TRACE_WRAPPER_H_

#include "heap/stubs.h"

namespace v8 {
class String;
}

namespace blink {

class A : public GarbageCollected<A> {
 public:
  void Trace(Visitor*) {
    // Missing visitor->Trace(str_);
  }

 private:
  TraceWrapperV8Reference<v8::String> str_;
};

class B : public GarbageCollected<B> {
 public:
  void Trace(Visitor* visitor);
  void TraceAfterDispatch(Visitor*) {}
};

class C : public B {
 public:
  void TraceAfterDispatch(Visitor*) {
    // Missing visitor->Trace(str_);
  }

 private:
  TraceWrapperV8Reference<v8::String> str_;
};

}  // namespace blink

#endif  // TRACE_WRAPPER_H_
