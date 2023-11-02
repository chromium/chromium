// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/animation_host.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(wm::AnimationHost*)

namespace wm {

DEFINE_UI_CLASS_PROPERTY_KEY(AnimationHost*, kRootWindowAnimationHostKey, NULL)

void SetAnimationHost(aura::Window* window, AnimationHost* animation_host) {
  DCHECK(window);
  window->SetProperty(kRootWindowAnimationHostKey, animation_host);
}

AnimationHost* GetAnimationHost(aura::Window* window) {
  DCHECK(window);
  return window->GetProperty(kRootWindowAnimationHostKey);
}

}  // namespace wm
