// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_EXPORT_H_
#define UI_GFX_COLOR_SPACE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(COLOR_SPACE_IMPLEMENTATION)
#define COLOR_SPACE_EXPORT __declspec(dllexport)
#else
#define COLOR_SPACE_EXPORT __declspec(dllimport)
#endif  // defined(COLOR_SPACE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(COLOR_SPACE_IMPLEMENTATION)
#define COLOR_SPACE_EXPORT __attribute__((visibility("default")))
#else
#define COLOR_SPACE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define COLOR_SPACE_EXPORT
#endif

#endif  // UI_GFX_COLOR_SPACE_EXPORT_H_
