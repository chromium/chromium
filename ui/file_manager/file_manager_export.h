// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_FILE_MANAGER_FILE_MANAGER_EXPORT_H_
#define UI_FILE_MANAGER_FILE_MANAGER_EXPORT_H_

// Defines FILE_MANAGER_EXPORT so that functionality implemented by the
// FILE_MANAGER module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(FILE_MANAGER_IMPLEMENTATION)
#define FILE_MANAGER_EXPORT __declspec(dllexport)
#else
#define FILE_MANAGER_EXPORT __declspec(dllimport)
#endif  // defined(FILE_MANAGER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(FILE_MANAGER_IMPLEMENTATION)
#define FILE_MANAGER_EXPORT __attribute__((visibility("default")))
#else
#define FILE_MANAGER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define FILE_MANAGER_EXPORT
#endif

#endif  // UI_FILE_MANAGER_FILE_MANAGER_EXPORT_H_
