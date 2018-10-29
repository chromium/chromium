// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_GEOMETRY_EXPORT_H_
#define UI_GFX_GEOMETRY_GEOMETRY_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GEOMETRY_IMPLEMENTATION)
#define GEOMETRY_EXPORT __declspec(dllexport)
#else
#define GEOMETRY_EXPORT __declspec(dllimport)
#endif  // defined(GEOMETRY_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GEOMETRY_IMPLEMENTATION)
#define GEOMETRY_EXPORT __attribute__((visibility("default")))
#else
#define GEOMETRY_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define GEOMETRY_EXPORT
#endif

#endif  // UI_GFX_GEOMETRY_GEOMETRY_EXPORT_H_
