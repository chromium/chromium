// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_PLATFORM_EVENT_H_
#define UI_EVENTS_PLATFORM_EVENT_H_

#include "build/build_config.h"

// TODO(crbug.com/40267204): Both gfx::NativeEvent and ui::PlatformEvent
// are typedefs for native event types on different platforms, but they're
// slightly different and used in different places. They should be merged.

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#elif BUILDFLAG(IS_APPLE)
#include "base/apple/owned_objc.h"
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
#elif BUILDFLAG(IS_MAC)
using PlatformEvent = base::apple::OwnedNSEvent;
#elif BUILDFLAG(IS_IOS)
using PlatformEvent = base::apple::OwnedUIEvent;
#else
using PlatformEvent = void*;
#endif

}  // namespace ui

#endif  // UI_EVENTS_PLATFORM_EVENT_H_
