// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_EXPORT_H_
#define UI_NATIVE_THEME_NATIVE_THEME_EXPORT_H_

// Defines NATIVE_THEME_EXPORT so that functionality implemented by the
// native_theme library can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(NATIVE_THEME_IMPLEMENTATION)
#define NATIVE_THEME_EXPORT __declspec(dllexport)
#else
#define NATIVE_THEME_EXPORT __declspec(dllimport)
#endif  // defined(NATIVE_THEME_IMPLEMENTATION)

#else  // !defined(WIN32)

#if defined(NATIVE_THEME_IMPLEMENTATION)
#define NATIVE_THEME_EXPORT __attribute__((visibility("default")))
#else
#define NATIVE_THEME_EXPORT
#endif

#endif  // defined(WIN32)

#else  // !defined(COMPONENT_BUILD)

#define NATIVE_THEME_EXPORT

#endif  // defined(COMPONENT_BUILD)

#endif  // UI_NATIVE_THEME_NATIVE_THEME_EXPORT_H_
