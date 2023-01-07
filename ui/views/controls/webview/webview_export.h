// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_EXPORT_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_EXPORT_H_

// Defines WEBVIEW_EXPORT so that functionality implemented by the webview
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEBVIEW_IMPLEMENTATION)
#define WEBVIEW_EXPORT __declspec(dllexport)
#else
#define WEBVIEW_EXPORT __declspec(dllimport)
#endif  // defined(WEBVIEW_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WEBVIEW_IMPLEMENTATION)
#define WEBVIEW_EXPORT __attribute__((visibility("default")))
#else
#define WEBVIEW_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WEBVIEW_EXPORT
#endif

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_EXPORT_H_
