// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_SCOPED_IGNORE_ERRORS_H_
#define UI_GFX_X_SCOPED_IGNORE_ERRORS_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/connection.h"

namespace x11 {

// Sets a no-op error handler for a connection while this class is alive.
class COMPONENT_EXPORT(X11) ScopedIgnoreErrors {
 public:
  explicit ScopedIgnoreErrors(Connection* connection);
  ~ScopedIgnoreErrors();

 private:
  const raw_ptr<Connection> connection_;
  Connection::ErrorHandler old_error_handler_;
};

}  // namespace x11

#endif  // UI_GFX_X_SCOPED_IGNORE_ERRORS_H_
