// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metrics.h"

namespace {

// Default double click interval in milliseconds.
// Same as what gtk uses.
const int kDefaultDoubleClickInterval = 500;

}  // namespace

namespace views {

int GetDoubleClickInterval() {
  return kDefaultDoubleClickInterval;
}

int GetMenuShowDelay() {
  return 0;
}

}  // namespace views
