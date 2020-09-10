// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_ui_data_impl.h"

namespace weblayer {

NavigationUIDataImpl::NavigationUIDataImpl(
    bool disable_network_error_auto_reload)
    : disable_network_error_auto_reload_(disable_network_error_auto_reload) {}

NavigationUIDataImpl::~NavigationUIDataImpl() = default;

std::unique_ptr<content::NavigationUIData> NavigationUIDataImpl::Clone() {
  return std::make_unique<NavigationUIDataImpl>(
      disable_network_error_auto_reload_);
}

}  // namespace weblayer
