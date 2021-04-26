// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_LOAD_COMPLETION_OBSERVER_H_
#define WEBLAYER_TEST_LOAD_COMPLETION_OBSERVER_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "weblayer/public/navigation_observer.h"

namespace weblayer {

class Shell;
class Tab;

// A helper that waits for the next load to complete.
class LoadCompletionObserver : public NavigationObserver {
 public:
  // Creates an instance that begins waiting for a load within |shell| to
  // complete.
  explicit LoadCompletionObserver(Shell* shell);
  ~LoadCompletionObserver() override;

  // Spins a RunLoop until the next load completes.
  void Wait();

 private:
  // NavigationObserver implementation:
  void LoadStateChanged(bool is_loading, bool to_different_document) override;

  Tab* tab_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(LoadCompletionObserver);
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_LOAD_COMPLETION_OBSERVER_H_
