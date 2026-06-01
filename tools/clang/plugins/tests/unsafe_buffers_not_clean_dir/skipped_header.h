// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIPPED_HEADER_H_
#define SKIPPED_HEADER_H_

template <typename T>
void unsafe_template_func(T* p) {
  p[5] = 0;  // This should warn
}

#endif  // SKIPPED_HEADER_H_
