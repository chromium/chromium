// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_EXPORT_H_
#define UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_EXPORT_H_

#if defined(WIN32)
#error Not for use on Windows.
#endif

#if defined(COMPONENT_BUILD)

#if defined(ACCELERATED_WIDGET_MAC_IMPLEMENTATION)
#define ACCELERATED_WIDGET_MAC_EXPORT __attribute__((visibility("default")))
#else
#define ACCELERATED_WIDGET_MAC_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define ACCELERATED_WIDGET_MAC_EXPORT
#endif

#endif  // UI_ACCELERATED_WIDGET_MAC_ACCELERATED_WIDGET_MAC_EXPORT_H_
