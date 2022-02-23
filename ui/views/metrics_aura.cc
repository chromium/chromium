// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/views/metrics.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace views {

int GetDoubleClickInterval() {
#if BUILDFLAG(IS_WIN)
  return ::GetDoubleClickTime();
#else
  // TODO(jennyz): This value may need to be adjusted on different platforms.
  const int kDefaultDoubleClickIntervalMs = 500;
  return kDefaultDoubleClickIntervalMs;
#endif
}

int GetMenuShowDelay() {
#if BUILDFLAG(IS_WIN)
  static DWORD delay = 0;
  if (!delay && !SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &delay, 0))
    delay = kDefaultMenuShowDelay;
  return delay;
#else
  return 0;
#endif
}

}  // namespace views
