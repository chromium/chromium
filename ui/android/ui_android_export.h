// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_UI_ANDROID_EXPORT_H_
#define UI_ANDROID_UI_ANDROID_EXPORT_H_

// Defines UI_ANDROID_EXPORT so that functionality implemented by the UI module
// can be exported to consumers.

#if defined(COMPONENT_BUILD)

#if defined(WIN32)
#error Unsupported target architecture.
#else  // !defined(WIN32)

#if defined(UI_ANDROID_IMPLEMENTATION)
#define UI_ANDROID_EXPORT __attribute__((visibility("default")))
#else
#define UI_ANDROID_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define UI_ANDROID_EXPORT

#endif

#endif  // UI_ANDROID_UI_ANDROID_EXPORT_H_
