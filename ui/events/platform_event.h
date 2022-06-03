// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_EVENT_H_
#define UI_EVENTS_PLATFORM_EVENT_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/windows_types.h"
#elif defined(OS_APPLE)
#if defined(__OBJC__)
@class NSEvent;
#else   // __OBJC__
class NSEvent;
#endif  // __OBJC__
#endif

namespace ui {
class Event;
}

namespace ui {

// Cross platform typedefs for native event types.
#if defined(USE_OZONE)
using PlatformEvent = ui::Event*;
#elif defined(OS_WIN)
using PlatformEvent = CHROME_MSG;
#elif defined(OS_APPLE)
using PlatformEvent = NSEvent*;
#else
using PlatformEvent = void*;
#endif

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_EVENT_H_
