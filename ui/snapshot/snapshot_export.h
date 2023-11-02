// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SNAPSHOT_EXPORT_H_
#define UI_SNAPSHOT_SNAPSHOT_EXPORT_H_

// Defines SNAPSHOT_EXPORT so that functionality implemented by the snapshot
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SNAPSHOT_IMPLEMENTATION)
#define SNAPSHOT_EXPORT __declspec(dllexport)
#else
#define SNAPSHOT_EXPORT __declspec(dllimport)
#endif  // defined(SNAPSHOT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(SNAPSHOT_IMPLEMENTATION)
#define SNAPSHOT_EXPORT __attribute__((visibility("default")))
#else
#define SNAPSHOT_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SNAPSHOT_EXPORT
#endif

#endif  // UI_SNAPSHOT_SNAPSHOT_EXPORT_H_
