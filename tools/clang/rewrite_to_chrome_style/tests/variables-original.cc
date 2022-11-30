// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Global variables
int frameCount = 0;
// Make sure that underscore-insertion doesn't get too confused by acronyms.
static int variableMentioningHTTPAndHTTPS = 1;
// g_ prefix, but doesn't follow Google style.
int g_withBlinkNaming;
// Already Google style, should not change.
int g_already_google_style_;

// Function parameters
int function(int interestingNumber) {
  // Local variables.
  int aLocalVariable = 1;
  // Static locals.
  static int aStaticLocalVariable = 2;
  // Make sure references to variables are also rewritten.
  return frameCount +
         variableMentioningHTTPAndHTTPS * interestingNumber / aLocalVariable %
             aStaticLocalVariable;
}

}  // namespace blink

using blink::frameCount;

int F() {
  // Make sure variables qualified with a namespace name are still rewritten
  // correctly.
  return frameCount + blink::frameCount;
}
