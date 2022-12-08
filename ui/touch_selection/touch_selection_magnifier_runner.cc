// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_magnifier_runner.h"

#include "base/check_op.h"

namespace ui {

namespace {

TouchSelectionMagnifierRunner* g_touch_selection_magnifier_runner = nullptr;

}  // namespace

TouchSelectionMagnifierRunner::~TouchSelectionMagnifierRunner() {
  DCHECK_EQ(this, g_touch_selection_magnifier_runner);
  g_touch_selection_magnifier_runner = nullptr;
}

TouchSelectionMagnifierRunner* TouchSelectionMagnifierRunner::GetInstance() {
  return g_touch_selection_magnifier_runner;
}

TouchSelectionMagnifierRunner::TouchSelectionMagnifierRunner() {
  DCHECK(!g_touch_selection_magnifier_runner);
  g_touch_selection_magnifier_runner = this;
}

}  // namespace ui
