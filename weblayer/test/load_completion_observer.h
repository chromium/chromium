// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_LOAD_COMPLETION_OBSERVER_H_
#define WEBLAYER_TEST_LOAD_COMPLETION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
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

  LoadCompletionObserver(const LoadCompletionObserver&) = delete;
  LoadCompletionObserver& operator=(const LoadCompletionObserver&) = delete;

  ~LoadCompletionObserver() override;

  // Spins a RunLoop until the next load completes.
  void Wait();

 private:
  // NavigationObserver implementation:
  void LoadStateChanged(bool is_loading, bool should_show_loading_ui) override;

  raw_ptr<Tab> tab_;
  base::RunLoop run_loop_;
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_LOAD_COMPLETION_OBSERVER_H_
