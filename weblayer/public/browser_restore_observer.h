// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_BROWSER_RESTORE_OBSERVER_H_
#define WEBLAYER_PUBLIC_BROWSER_RESTORE_OBSERVER_H_

#include "base/observer_list_types.h"

namespace weblayer {

// Used for observing events related to restoring the previous state of a
// Browser.
class BrowserRestoreObserver : public base::CheckedObserver {
 public:
  // Called when the Browser has completed restoring the previous state.
  virtual void OnRestoreCompleted() {}

 protected:
  ~BrowserRestoreObserver() override = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_RESTORE_OBSERVER_H_
