// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_VIEWS_EXAMPLES_WITH_CONTENT_EXPORT_H_
#define UI_VIEWS_EXAMPLES_VIEWS_EXAMPLES_WITH_CONTENT_EXPORT_H_

// Defines VIEWS_EXAMPLES_WITH_CONTENT_EXPORT so that functionality implemented
// by the views_examples_with_content_lib module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIEWS_EXAMPLES_WITH_CONTENT_IMPLEMENTATION)
#define VIEWS_EXAMPLES_WITH_CONTENT_EXPORT __declspec(dllexport)
#else
#define VIEWS_EXAMPLES_WITH_CONTENT_EXPORT __declspec(dllimport)
#endif  // defined(VIEWS_EXAMPLES_WITH_CONTENT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VIEWS_EXAMPLES_WITH_CONTENT_IMPLEMENTATION)
#define VIEWS_EXAMPLES_WITH_CONTENT_EXPORT \
  __attribute__((visibility("default")))
#else
#define VIEWS_EXAMPLES_WITH_CONTENT_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define VIEWS_EXAMPLES_WITH_CONTENT_EXPORT
#endif

#endif  // UI_VIEWS_EXAMPLES_VIEWS_EXAMPLES_WITH_CONTENT_EXPORT_H_
