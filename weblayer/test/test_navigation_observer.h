// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_TEST_NAVIGATION_OBSERVER_H_
#define WEBLAYER_TEST_TEST_NAVIGATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "weblayer/public/navigation_observer.h"

namespace weblayer {

class Shell;
class Tab;

// A helper that waits for a navigation to finish.
class TestNavigationObserver : public NavigationObserver {
 public:
  enum class NavigationEvent { kStart, kCompletion, kFailure };

  // Creates an instance that begins waiting for a Navigation within |shell| and
  // to |url| to reach the specified |target_event|.
  TestNavigationObserver(const GURL& url,
                         NavigationEvent target_event,
                         Shell* shell);
  TestNavigationObserver(const GURL& url,
                         NavigationEvent target_event,
                         Tab* tab);

  TestNavigationObserver(const TestNavigationObserver&) = delete;
  TestNavigationObserver& operator=(const TestNavigationObserver&) = delete;

  ~TestNavigationObserver() override;

  // Spins a RunLoop until the requested type of navigation event is observed.
  void Wait();

 private:
  // NavigationObserver implementation:
  void NavigationStarted(Navigation* navigation) override;
  void NavigationCompleted(Navigation* navigation) override;
  void NavigationFailed(Navigation* navigation) override;
  void LoadStateChanged(bool is_loading, bool should_show_loading_ui) override;

  void CheckNavigationCompleted();

  const GURL url_;
  absl::optional<NavigationEvent> observed_event_;
  NavigationEvent target_event_;
  raw_ptr<Tab> tab_;
  bool done_loading_ = false;
  base::RunLoop run_loop_;
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_TEST_NAVIGATION_OBSERVER_H_
