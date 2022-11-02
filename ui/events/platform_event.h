// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_EVENT_H_
#define UI_EVENTS_PLATFORM_EVENT_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#elif BUILDFLAG(IS_APPLE)
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
#if BUILDFLAG(IS_OZONE)
using PlatformEvent = ui::Event*;
#elif BUILDFLAG(IS_WIN)
using PlatformEvent = CHROME_MSG;
#elif BUILDFLAG(IS_APPLE)
using PlatformEvent = NSEvent*;
#else
using PlatformEvent = void*;
#endif

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_EVENT_H_
