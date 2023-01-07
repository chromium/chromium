// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/closure_animation_observer.h"

namespace ui {

ClosureAnimationObserver::ClosureAnimationObserver(base::OnceClosure closure)
    : closure_(std::move(closure)) {
  DCHECK(!closure_.is_null());
}

ClosureAnimationObserver::~ClosureAnimationObserver() {
}

void ClosureAnimationObserver::OnImplicitAnimationsCompleted() {
  std::move(closure_).Run();
  delete this;
}

}  // namespace ui
