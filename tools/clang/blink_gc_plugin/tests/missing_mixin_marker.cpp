// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "heap/stubs.h"

namespace blink {

class Mixin : public GarbageCollectedMixin {
 public:
  virtual void Trace(Visitor*) {}
};

// Class derived from a mixin needs USING_GARBAGE_COLLECTED_MIXIN.
class Derived : public GarbageCollected<Derived>, public Mixin {
  virtual void Trace(Visitor* visitor) override { Mixin::Trace(visitor); }
};

template <typename T>
class Supplement : public GarbageCollectedMixin {
 public:
  virtual void Trace(Visitor*) {}
};

// Class derived from a mixin template needs USING_GARBAGE_COLLECTED_MIXIN.
class MySupplement : public GarbageCollected<MySupplement>,
                     public Supplement<Derived> {
  virtual void Trace(Visitor* visitor) override { Supplement::Trace(visitor); }
};

// This is the right way to do it.
// We should get no warning either here or at a forward declaration.
class GoodDerived : public GarbageCollected<GoodDerived>, public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN(GoodDerived);
};
class GoodDerived;

// Same macro providing only a typedef is also ok.
class GoodDerivedMacroUsingTypedef
    : public GarbageCollected<GoodDerivedMacroUsingTypedef>,
      public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN_NEW(GoodDerivedMacroUsingTypedef);
};
class GoodDerivedMacroUsingTypedef;

// Abstract classes (i.e. ones with pure virtual methods) can't be constructed
// and so it's assumed their derived classes will have
// USING_GARBAGE_COLLECTED_MIXIN.
class PureVirtual : public GarbageCollected<PureVirtual>, public Mixin {
 public:
  virtual void Foo() = 0;
};
// ...but failure to do so is still bad. Warn here.
class PureVirtualDerived : public PureVirtual {
 public:
  void Foo() override {}
};

// And there's an exception for "same size" classes, which are just used for
// assertions. This should not warn.
class SameSizeAsGoodDerived : public GarbageCollected<SameSizeAsGoodDerived>,
                              public Mixin {
  char same_size_as_mixin_marker_[1];
};

}  // namespace blink
