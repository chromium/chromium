// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_WEB_DIALOGS_EXPORT_H_
#define UI_WEB_DIALOGS_WEB_DIALOGS_EXPORT_H_

// Defines WEB_DIALOGS_EXPORT so that functionality implemented by the
// web_dialogs module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEB_DIALOGS_IMPLEMENTATION)
#define WEB_DIALOGS_EXPORT __declspec(dllexport)
#else
#define WEB_DIALOGS_EXPORT __declspec(dllimport)
#endif  // defined(WEB_DIALOGS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WEB_DIALOGS_IMPLEMENTATION)
#define WEB_DIALOGS_EXPORT __attribute__((visibility("default")))
#else
#define WEB_DIALOGS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WEB_DIALOGS_EXPORT
#endif

#endif  // UI_WEB_DIALOGS_WEB_DIALOGS_EXPORT_H_
