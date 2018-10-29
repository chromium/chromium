// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_PLATFORM_EXPORT_H_
#define UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_PLATFORM_EXPORT_H_

// Defines WM_PLATFORM_EXPORT so that functionality implemented by the
// wm_platform module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PLATFORM_WINDOW_HANDLER_IMPLEMENTATION)
#define WM_PLATFORM_EXPORT __declspec(dllexport)
#else
#define WM_PLATFORM_EXPORT __declspec(dllimport)
#endif  // defined(WM_PLATFORM_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PLATFORM_WINDOW_HANDLER_IMPLEMENTATION)
#define WM_PLATFORM_EXPORT __attribute__((visibility("default")))
#else
#define WM_PLATFORM_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WM_PLATFORM_EXPORT
#endif

#endif  // UI_PLATFORM_WINDOW_PLATFORM_WINDOW_HANDLER_WM_PLATFORM_EXPORT_H_
