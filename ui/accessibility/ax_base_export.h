// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_BASE_EXPORT_H_
#define UI_ACCESSIBILITY_AX_BASE_EXPORT_H_

// Defines AX_BASE_EXPORT so that functionality implemented by the
// ui/accessibility:ax_base module can be exported to consumers.

#if defined(COMPONENT_BUILD) && defined(WIN32) && \
    defined(AX_BASE_IMPLEMENTATION)
#define AX_BASE_EXPORT __declspec(dllexport)
#elif defined(COMPONENT_BUILD) && defined(WIN32)
#define AX_BASE_EXPORT __declspec(dllimport)
#elif defined(COMPONENT_BUILD) && !defined(WIN32) && \
    defined(AX_BASE_IMPLEMENTATION)
#define AX_BASE_EXPORT __attribute__((visibility("default")))
#else
#define AX_BASE_EXPORT
#endif

#endif  // UI_ACCESSIBILITY_AX_BASE_EXPORT_H_
