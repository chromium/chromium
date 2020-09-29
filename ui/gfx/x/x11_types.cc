// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/x11_types.h"

#include <string.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_switches.h"

namespace gfx {

XDisplay* GetXDisplay() {
  return x11::Connection::Get()->display();
}

}  // namespace gfx
