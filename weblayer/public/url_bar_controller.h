// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_URL_BAR_CONTROLLER_H_
#define WEBLAYER_PUBLIC_URL_BAR_CONTROLLER_H_

#include <string>

#include "components/security_state/core/security_state.h"

namespace weblayer {

class Browser;

class UrlBarController {
 public:
  static std::unique_ptr<UrlBarController> Create(Browser* browser);

  virtual ~UrlBarController() {}
  virtual std::u16string GetUrlForDisplay() = 0;
  virtual security_state::SecurityLevel GetConnectionSecurityLevel() = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_URL_BAR_CONTROLLER_H_