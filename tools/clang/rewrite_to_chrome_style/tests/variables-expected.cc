// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Global variables
int g_frame_count = 0;
// Make sure that underscore-insertion doesn't get too confused by acronyms.
static int g_variable_mentioning_http_and_https = 1;
// g_ prefix, but doesn't follow Google style.
int g_with_blink_naming;
// Already Google style, should not change.
int g_already_google_style_;

// Function parameters
int Function(int interesting_number) {
  // Local variables.
  int a_local_variable = 1;
  // Static locals.
  static int a_static_local_variable = 2;
  // Make sure references to variables are also rewritten.
  return g_frame_count + g_variable_mentioning_http_and_https *
                             interesting_number / a_local_variable %
                             a_static_local_variable;
}

}  // namespace blink

using blink::g_frame_count;

int F() {
  // Make sure variables qualified with a namespace name are still rewritten
  // correctly.
  return g_frame_count + blink::g_frame_count;
}
