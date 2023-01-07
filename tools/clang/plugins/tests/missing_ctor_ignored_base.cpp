// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "missing_ctor_ignored_base.h"

int main() {
  MissingCtorsWithIgnoredBase one;
  MissingCtorsWithIgnoredGrandBase two;
  return 0;
}
