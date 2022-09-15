// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/activation_client.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(wm::ActivationClient*)

namespace wm {

DEFINE_UI_CLASS_PROPERTY_KEY(ActivationClient*,
                             kRootWindowActivationClientKey,
                             NULL)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideOnDeactivate, false)

void SetActivationClient(aura::Window* root_window, ActivationClient* client) {
  root_window->SetProperty(kRootWindowActivationClientKey, client);
}

const ActivationClient* GetActivationClient(const aura::Window* root_window) {
  return root_window ? root_window->GetProperty(kRootWindowActivationClientKey)
                     : nullptr;
}

ActivationClient* GetActivationClient(aura::Window* root_window) {
  return root_window ? root_window->GetProperty(kRootWindowActivationClientKey)
                     : nullptr;
}

void SetHideOnDeactivate(aura::Window* window, bool hide_on_deactivate) {
  window->SetProperty(kHideOnDeactivate, hide_on_deactivate);
}

bool GetHideOnDeactivate(aura::Window* window) {
  return window->GetProperty(kHideOnDeactivate);
}

}  // namespace wm
