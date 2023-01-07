// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_LOG_NOOP_H_
#define UI_GTK_LOG_NOOP_H_

// This is a no-op logger used to ignore error messages reported by generated
// stub initializers.  Some missing symbols are expected since they may only be
// available in specific versions of GTK.
struct LogNoop {
  template <typename T>
  LogNoop operator<<(const T& t) {
    return *this;
  }
};

#endif  // UI_GTK_LOG_NOOP_H_
