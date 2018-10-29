// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BRIDGE_MAC_VIEWS_BRIDGE_MAC_EXPORT_H_
#define UI_VIEWS_BRIDGE_MAC_VIEWS_BRIDGE_MAC_EXPORT_H_

// Defines VIEWS_BRIDGE_MAC_EXPORT so that functionality implemented by the
// RemoteMacViews module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIEWS_BRIDGE_MAC_IMPLEMENTATION)
#define VIEWS_BRIDGE_MAC_EXPORT __declspec(dllexport)
#else
#define VIEWS_BRIDGE_MAC_EXPORT __declspec(dllimport)
#endif  // defined(VIEWS_BRIDGE_MAC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VIEWS_BRIDGE_MAC_IMPLEMENTATION)
#define VIEWS_BRIDGE_MAC_EXPORT __attribute__((visibility("default")))
#else
#define VIEWS_BRIDGE_MAC_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define VIEWS_BRIDGE_MAC_EXPORT
#endif

#endif  // UI_VIEWS_BRIDGE_MAC_VIEWS_BRIDGE_MAC_EXPORT_H_
