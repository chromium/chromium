// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

struct Op {
  bool operator==(const Op&) { return true; }
};

struct Op2 {};

inline bool operator==(const Op2&, const Op2) {
  return true;
}

}  // namespace

void G() {
  blink::Op a, b;
  bool c = a == b;

  blink::Op2 a2, b2;
  bool c2 = a2 == b2;
}
