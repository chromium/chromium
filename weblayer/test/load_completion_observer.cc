// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/load_completion_observer.h"

#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"

namespace weblayer {

LoadCompletionObserver::LoadCompletionObserver(Shell* shell)
    : tab_(shell->tab()) {
  tab_->GetNavigationController()->AddObserver(this);
}

LoadCompletionObserver::~LoadCompletionObserver() {
  tab_->GetNavigationController()->RemoveObserver(this);
}

void LoadCompletionObserver::LoadStateChanged(bool is_loading,
                                              bool to_different_document) {
  if (!is_loading)
    run_loop_.Quit();
}

void LoadCompletionObserver::Wait() {
  run_loop_.Run();
}

}  // namespace weblayer
