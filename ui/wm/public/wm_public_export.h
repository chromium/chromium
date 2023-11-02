// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_WM_PUBLIC_EXPORT_H_
#define UI_WM_PUBLIC_WM_PUBLIC_EXPORT_H_

// Defines WM_PUBLIC_EXPORT so that functionality implemented by the wm_public
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WM_PUBLIC_IMPLEMENTATION)
#define WM_PUBLIC_EXPORT __declspec(dllexport)
#else
#define WM_PUBLIC_EXPORT __declspec(dllimport)
#endif  // defined(WM_PUBLIC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WM_PUBLIC_IMPLEMENTATION)
#define WM_PUBLIC_EXPORT __attribute__((visibility("default")))
#else
#define WM_PUBLIC_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WM_PUBLIC_EXPORT
#endif

#endif  // UI_WM_PUBLIC_WM_PUBLIC_EXPORT_H_
