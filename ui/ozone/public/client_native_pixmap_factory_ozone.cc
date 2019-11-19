// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"

#include <memory>

#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"

namespace ui {

std::unique_ptr<gfx::ClientNativePixmapFactory>
CreateClientNativePixmapFactoryOzone() {
  TRACE_EVENT1("ozone", "CreateClientNativePixmapFactoryOzone", "platform",
               GetOzonePlatformName());
  return PlatformObject<gfx::ClientNativePixmapFactory>::Create();
}

}  // namespace ui
