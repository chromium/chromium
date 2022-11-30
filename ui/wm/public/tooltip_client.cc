// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/tooltip_client.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_PUBLIC_EXPORT, wm::TooltipClient*)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_PUBLIC_EXPORT, void**)

namespace wm {

DEFINE_UI_CLASS_PROPERTY_KEY(TooltipClient*, kRootWindowTooltipClientKey, NULL)
DEFINE_UI_CLASS_PROPERTY_KEY(std::u16string*, kTooltipTextKey, NULL)
DEFINE_UI_CLASS_PROPERTY_KEY(void*, kTooltipIdKey, NULL)

void SetTooltipClient(aura::Window* root_window, TooltipClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kRootWindowTooltipClientKey, client);
}

TooltipClient* GetTooltipClient(aura::Window* root_window) {
  if (root_window)
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  return root_window ?
      root_window->GetProperty(kRootWindowTooltipClientKey) : NULL;
}

void SetTooltipText(aura::Window* window, std::u16string* tooltip_text) {
  window->SetProperty(kTooltipTextKey, tooltip_text);
}

void SetTooltipId(aura::Window* window, void* id) {
  if (id != GetTooltipId(window))
    window->SetProperty(kTooltipIdKey, id);
}

const std::u16string GetTooltipText(aura::Window* window) {
  std::u16string* string_ptr = window->GetProperty(kTooltipTextKey);
  return string_ptr ? *string_ptr : std::u16string();
}

const void* GetTooltipId(aura::Window* window) {
  return window->GetProperty(kTooltipIdKey);
}

}  // namespace wm
