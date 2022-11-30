// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/tutorial/hello.h"

#include <stdio.h>

void Hello(const char* who) {
  printf("Hello, %s.\n", who);
}

#if defined(TWO_PEOPLE)
void Hello(const char* one, const char* two) {
  printf("Hello, %s and %s.\n", one, two);
}
#endif
