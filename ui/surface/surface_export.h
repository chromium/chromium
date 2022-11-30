// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SURFACE_SURFACE_EXPORT_H_
#define UI_SURFACE_SURFACE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SURFACE_IMPLEMENTATION)
#define SURFACE_EXPORT __declspec(dllexport)
#else
#define SURFACE_EXPORT __declspec(dllimport)
#endif  // defined(SURFACE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SURFACE_IMPLEMENTATION)
#define SURFACE_EXPORT __attribute__((visibility("default")))
#else
#define SURFACE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SURFACE_EXPORT
#endif

#endif  // UI_SURFACE_SURFACE_EXPORT_H_
