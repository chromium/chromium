// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_EXPORT_H_
#define UI_VIEWS_VIEWS_EXPORT_H_

// Defines VIEWS_EXPORT so that functionality implemented by the Views module
// can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIEWS_IMPLEMENTATION)
#define VIEWS_EXPORT __declspec(dllexport)
#else
#define VIEWS_EXPORT __declspec(dllimport)
#endif  // defined(VIEWS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VIEWS_IMPLEMENTATION)
#define VIEWS_EXPORT __attribute__((visibility("default")))
#else
#define VIEWS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define VIEWS_EXPORT
#endif

#endif  // UI_VIEWS_VIEWS_EXPORT_H_
