// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_EXPORT_H_
#define UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(STUB_WINDOW_IMPLEMENTATION)
#define STUB_WINDOW_EXPORT __declspec(dllexport)
#else
#define STUB_WINDOW_EXPORT __declspec(dllimport)
#endif  // defined(STUB_WINDOW_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(STUB_WINDOW_IMPLEMENTATION)
#define STUB_WINDOW_EXPORT __attribute__((visibility("default")))
#else
#define STUB_WINDOW_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define STUB_WINDOW_EXPORT
#endif

#endif  // UI_PLATFORM_WINDOW_STUB_STUB_WINDOW_EXPORT_H_
