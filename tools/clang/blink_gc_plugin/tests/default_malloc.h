// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEFAULT_MALLOC_H_
#define DEFAULT_MALLOC_H_

#include "heap/stubs.h"

namespace blink {

class DefaultMallocWithImplicitDefaultConstructor {
  int a_ = 0;
};

class DefaultMallocWithExplicitDefaultConstructor {
 public:
  DefaultMallocWithExplicitDefaultConstructor() = default;

 private:
  int a = 0;
};

class DefaultMallocWithNonDefaultConstructor {
 public:
  explicit DefaultMallocWithNonDefaultConstructor(int a) : a_(a) {}

 private:
  int a_ = 0;

  // This is OK.
  struct {
    int b = 0;
  } b_;
};

// This is OK.
class AbstractClass {
 public:
  virtual ~AbstractClass(){};
  virtual void VirtualMethod() = 0;
};

// This is OK.
class DefaultMallocWithPrivateConstructor {
 private:
  DefaultMallocWithPrivateConstructor(int) {}
};

// This is OK.
class OverrideNew {
 public:
  void* operator new(size_t);
};

// This is OK.
class DeleteNew {
 public:
  void* operator new(size_t) = delete;
};

// This is OK.
class OverrideNewDerived : public OverrideNew {};

// This is OK.
class DeleteNewDerived : public DeleteNew {};

// All other test cases in the same directory are OK cases.

}  // namespace blink

#endif  // DEFAULT_MALLOC_H_
