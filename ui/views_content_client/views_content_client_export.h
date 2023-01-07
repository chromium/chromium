// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_EXPORT_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_EXPORT_H_

// Defines VIEWS_CONTENT_CLIENT_EXPORT so that functionality implemented by the
// views_content_client module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIEWS_CONTENT_CLIENT_IMPLEMENTATION)
#define VIEWS_CONTENT_CLIENT_EXPORT __declspec(dllexport)
#else
#define VIEWS_CONTENT_CLIENT_EXPORT __declspec(dllimport)
#endif  // defined(VIEWS_CONTENT_CLIENT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VIEWS_CONTENT_CLIENT_IMPLEMENTATION)
#define VIEWS_CONTENT_CLIENT_EXPORT __attribute__((visibility("default")))
#else
#define VIEWS_CONTENT_CLIENT_EXPORT
#endif
#endif  // defined(VIEWS_CONTENT_CLIENT_IMPLEMENTATION)

#else  // defined(COMPONENT_BUILD)
#define VIEWS_CONTENT_CLIENT_EXPORT
#endif

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_EXPORT_H_
