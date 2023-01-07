// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_UI_TOUCH_SELECTION_EXPORT_H_
#define UI_TOUCH_SELECTION_UI_TOUCH_SELECTION_EXPORT_H_

// Defines UI_TOUCH_SELECTION_EXPORT so that functionality implemented by the UI
// touch selection module can be exported to consumers.

#include "build/build_config.h"

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(UI_TOUCH_SELECTION_IMPLEMENTATION)
#define UI_TOUCH_SELECTION_EXPORT __declspec(dllexport)
#else
#define UI_TOUCH_SELECTION_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(UI_TOUCH_SELECTION_IMPLEMENTATION)
#define UI_TOUCH_SELECTION_EXPORT __attribute__((visibility("default")))
#else
#define UI_TOUCH_SELECTION_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define UI_TOUCH_SELECTION_EXPORT

#endif

#endif  // UI_TOUCH_SELECTION_UI_TOUCH_SELECTION_EXPORT_H_
