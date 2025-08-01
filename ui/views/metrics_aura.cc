// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/views/metrics.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace views {

base::TimeDelta GetDoubleClickInterval() {
#if BUILDFLAG(IS_WIN)
  return base::Milliseconds(::GetDoubleClickTime());
#else
  // TODO(jennyz): This value may need to be adjusted on different platforms.
  constexpr base::TimeDelta kDefaultDoubleClickInterval =
      base::Milliseconds(500);
  return kDefaultDoubleClickInterval;
#endif
}

base::TimeDelta GetMenuShowDelay() {
#if BUILDFLAG(IS_WIN)
  static base::TimeDelta delay = []() {
    DWORD show_delay;
    return SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &show_delay, 0)
               ? base::Milliseconds(show_delay)
               : kDefaultMenuShowDelay;
  }();
  return delay;
#else
  return base::Milliseconds(0);
#endif
}

}  // namespace views
