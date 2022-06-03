// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRACEIMPL_DEPENDENT_SCOPE_H_
#define TRACEIMPL_DEPENDENT_SCOPE_H_

#include "heap/stubs.h"

namespace blink {

class X : public GarbageCollected<X> {
 public:
  virtual void Trace(Visitor*) const {}
};

template <typename T>
class Base : public GarbageCollected<Base<T> > {
 public:
  virtual void Trace(Visitor* visitor) const {}
};

template <typename T>
class Derived : public Base<T> {
 public:
  void Trace(Visitor* visitor) const override { Base<T>::Trace(visitor); }
};

template <typename T>
class DerivedMissingTrace : public Base<T> {
 public:
  void Trace(Visitor* visitor) const override {
    // Missing Base<T>::Trace(visitor).
  }
};

}

#endif  // TRACEIMPL_DEPENDENT_SCOPE_H_
