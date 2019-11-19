// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_TEST_NAVIGATION_OBSERVER_H_
#define WEBLAYER_TEST_TEST_NAVIGATION_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "url/gurl.h"
#include "weblayer/public/navigation_observer.h"

namespace weblayer {

class Shell;
class Tab;

// A helper that waits for a navigation to finish.
class TestNavigationObserver : public NavigationObserver {
 public:
  enum class NavigationEvent { Completion, Failure };

  // Creates an instance that begins waiting for a Navigation within |shell| and
  // to |url| to either complete or fail as per |target_event|.
  TestNavigationObserver(const GURL& url,
                         NavigationEvent target_event,
                         Shell* shell);
  ~TestNavigationObserver() override;

  // Spins a RunLoop until the requested type of navigation event is observed.
  void Wait();

 private:
  // NavigationObserver implementation:
  void NavigationCompleted(Navigation* navigation) override;
  void NavigationFailed(Navigation* navigation) override;
  void LoadStateChanged(bool is_loading, bool to_different_document) override;

  void CheckNavigationCompleted();

  const GURL url_;
  base::Optional<NavigationEvent> observed_event_;
  NavigationEvent target_event_;
  Tab* tab_;
  bool done_loading_ = false;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationObserver);
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_TEST_NAVIGATION_OBSERVER_H_
