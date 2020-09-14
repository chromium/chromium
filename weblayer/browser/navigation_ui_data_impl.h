// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_UI_DATA_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_UI_DATA_IMPL_H_

#include "content/public/browser/navigation_ui_data.h"

namespace weblayer {

// Data that we pass to content::NavigationController::LoadURLWithParams
// and can access from content::NavigationHandle later.
class NavigationUIDataImpl : public content::NavigationUIData {
 public:
  explicit NavigationUIDataImpl(bool disable_network_error_auto_reload);
  NavigationUIDataImpl(const NavigationUIDataImpl&) = delete;
  NavigationUIDataImpl& operator=(const NavigationUIDataImpl&) = delete;
  ~NavigationUIDataImpl() override;

  // content::NavigationUIData implementation:
  std::unique_ptr<content::NavigationUIData> Clone() override;

  bool disable_network_error_auto_reload() const {
    return disable_network_error_auto_reload_;
  }

 private:
  bool disable_network_error_auto_reload_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_UI_DATA_IMPL_H_
