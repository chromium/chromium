// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define VIRTUAL virtual
#define VIRTUAL_VOID virtual void

class A {
 public:
  virtual void F() final {}
  // Make sure an out-of-place virtual doesn't cause an incorrect fixit removal
  // to be emitted.
  void virtual G() final {}
  // Don't emit any fixits for virtual from macros.
  VIRTUAL void H() final {}
  void VIRTUAL I() final {}
  VIRTUAL_VOID J() final {}
};
