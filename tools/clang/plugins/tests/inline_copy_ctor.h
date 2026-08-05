// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INLINE_COPY_CTOR_H_
#define INLINE_COPY_CTOR_H_

struct C {
  C();
  ~C();

  static C foo() { return C(); }

  int a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p , q, r, s, t, u, v, w, x;
};

#endif  // INLINE_COPY_CTOR_H_
