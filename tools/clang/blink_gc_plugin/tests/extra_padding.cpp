// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test relies on a 64bit target (test.py sets the triple explicitly).

#include "heap/stubs.h"

namespace blink {

class Object : public GarbageCollected<Object> {
  void Trace(Visitor*) const {}
};

namespace {

struct DisallowNewWithPadding {
  DISALLOW_NEW();

 public:
  virtual void Trace(Visitor* v) const {
    v->Trace(a);
    v->Trace(b);
  }

  // The plugin should warn that reordering would make sense here.
  Member<Object> a;
  void* raw;
  Member<Object> b;
};

struct DisallowNewWithoutPadding {
  DISALLOW_NEW();

 public:
  virtual void Trace(Visitor* v) const {
    v->Trace(a);
    v->Trace(b);
  }

  // The plugin shouldn't warn, e.g. reordering wouldn't eliminate padding.
  Member<Object> a;
  Member<Object> b;
  void* raw;
};

// Don't warn for templates until instantiated.
template <class T>
struct DisallowNewWithPaddingTemplate {
  DISALLOW_NEW();

 public:
  virtual void Trace(Visitor* v) const {
    v->Trace(a);
    v->Trace(b);
  }

  Member<Object> a;
  void* raw;
  Member<Object> b;
};

template struct DisallowNewWithPaddingTemplate<int>;

// A GarbageCollected class shall not be checked.
class GCed : GarbageCollected<GCed> {
  void Trace(Visitor* v) const {
    v->Trace(a);
    v->Trace(b);
  }

  Member<Object> a;
  void* raw;
  Member<Object> b;
};

// Explicitly setting the alignment requirement on fields should disable the
// check.
struct DisallowNewWithExplicitAlignment {
  DISALLOW_NEW();

  void Trace(Visitor* v) const {
    v->Trace(a);
    v->Trace(b);
  }

  Member<Object> a;
  void* raw;
  alignas(32) Member<Object> b;
};

// Disable the check with classes containing bitfields.
struct DisallowNewWithBitfield {
  DISALLOW_NEW();

  void Trace(Visitor* v) const {
    v->Trace(a);
    v->Trace(b);
  }

  Member<Object> a;
  void* raw;
  int b1 : 1;
  int b2 : 2;
  Member<Object> b;
};

// Disable the check with classes containing [[no_unique_address]].
struct DisallowNewWithNoUniqueAddress {
  DISALLOW_NEW();

  class Empty {};

  void Trace(Visitor* v) const {
    v->Trace(a);
    v->Trace(b);
  }

  Member<Object> a;
  void* raw;
  [[no_unique_address]] Empty empty;
  Member<Object> b;
};

}  // namespace

}  // namespace blink
