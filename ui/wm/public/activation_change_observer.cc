// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/activation_change_observer.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(wm::ActivationChangeObserver*)

namespace wm {

DEFINE_UI_CLASS_PROPERTY_KEY(ActivationChangeObserver*,
                             kActivationChangeObserverKey,
                             nullptr)

ActivationChangeObserver::~ActivationChangeObserver() {
  CHECK(!IsInObserverList());
}

void SetActivationChangeObserver(aura::Window* window,
                                 ActivationChangeObserver* observer) {
  window->SetProperty(kActivationChangeObserverKey, observer);
}

ActivationChangeObserver* GetActivationChangeObserver(aura::Window* window) {
  return window ? window->GetProperty(kActivationChangeObserverKey) : NULL;
}

}  // namespace wm
