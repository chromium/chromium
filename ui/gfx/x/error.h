// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_ERROR_H_
#define UI_GFX_X_ERROR_H_

#include <string>

#include "base/component_export.h"

namespace x11 {

class COMPONENT_EXPORT(X11) Error {
 public:
  Error();
  virtual ~Error();

  virtual std::string ToString() const = 0;

 private:
};

}  // namespace x11

#endif  // UI_GFX_X_EVENT_H_
