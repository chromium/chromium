// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_X11_EVENTS_DEVICES_X11_EXPORT_H_
#define UI_EVENTS_DEVICES_X11_EVENTS_DEVICES_X11_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(EVENTS_DEVICES_X11_IMPLEMENTATION)
#define EVENTS_DEVICES_X11_EXPORT __declspec(dllexport)
#else
#define EVENTS_DEVICES_X11_EXPORT __declspec(dllimport)
#endif  // defined(EVENTS_DEVICES_X11_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(EVENTS_DEVICES_X11_IMPLEMENTATION)
#define EVENTS_DEVICES_X11_EXPORT __attribute__((visibility("default")))
#else
#define EVENTS_DEVICES_X11_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define EVENTS_DEVICES_X11_EXPORT
#endif

#endif  // UI_EVENTS_DEVICES_X11_EVENTS_DEVICES_X11_EXPORT_H_
