// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/touch_editing_controller.h"

namespace ui {

namespace {
TouchEditingControllerFactory* g_shared_instance = NULL;
}  // namespace

TouchEditingControllerDeprecated* TouchEditingControllerDeprecated::Create(
    TouchEditable* client_view) {
  if (g_shared_instance)
    return g_shared_instance->Create(client_view);
  return NULL;
}

// static
void TouchEditingControllerFactory::SetInstance(
    TouchEditingControllerFactory* instance) {
  g_shared_instance = instance;
}

}  // namespace ui
