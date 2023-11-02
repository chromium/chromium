// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_menu_runner.h"

#include "base/check_op.h"

namespace ui {
namespace {

TouchSelectionMenuRunner* g_touch_selection_menu_runner = nullptr;

}  // namespace

TouchSelectionMenuClient::TouchSelectionMenuClient() = default;

TouchSelectionMenuClient::~TouchSelectionMenuClient() = default;

base::WeakPtr<TouchSelectionMenuClient> TouchSelectionMenuClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

TouchSelectionMenuRunner::~TouchSelectionMenuRunner() {
  DCHECK_EQ(this, g_touch_selection_menu_runner);
  g_touch_selection_menu_runner = nullptr;
}

TouchSelectionMenuRunner* TouchSelectionMenuRunner::GetInstance() {
  return g_touch_selection_menu_runner;
}

TouchSelectionMenuRunner::TouchSelectionMenuRunner() {
  DCHECK(!g_touch_selection_menu_runner);
  g_touch_selection_menu_runner = this;
}

}  // namespace ui
