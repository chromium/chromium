// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_selection_menu_runner.h"

#include <set>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"

namespace ui {
namespace {

TouchSelectionMenuRunner* g_touch_selection_menu_runner = nullptr;

// TODO(jamescook): Remove after investigation of https://crbug.com/1146270
std::set<TouchSelectionMenuClient*>& ValidClients() {
  static base::NoDestructor<std::set<TouchSelectionMenuClient*>> valid_clients;
  return *valid_clients;
}

}  // namespace

TouchSelectionMenuClient::TouchSelectionMenuClient() {
  ValidClients().insert(this);
}

TouchSelectionMenuClient::~TouchSelectionMenuClient() {
  ValidClients().erase(this);
}

// static
bool TouchSelectionMenuClient::IsValid(TouchSelectionMenuClient* client) {
  return base::Contains(ValidClients(), client);
}

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
