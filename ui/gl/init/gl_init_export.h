// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_INIT_EXPORT_H_
#define UI_GL_INIT_GL_INIT_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GL_INIT_IMPLEMENTATION)
#define GL_INIT_EXPORT __declspec(dllexport)
#else
#define GL_INIT_EXPORT __declspec(dllimport)
#endif  // defined(GL_INIT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GL_INIT_IMPLEMENTATION)
#define GL_INIT_EXPORT __attribute__((visibility("default")))
#else
#define GL_INIT_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define GL_INIT_EXPORT
#endif

#endif  // UI_GL_INIT_GL_INIT_EXPORT_H_
