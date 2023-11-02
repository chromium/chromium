// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_BUFFER_TYPES_GFX_IPC_EXPORT_H_
#define UI_GFX_IPC_BUFFER_TYPES_GFX_IPC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GFX_IPC_BUFFER_TYPES_IMPLEMENTATION)
#define GFX_IPC_BUFFER_TYPES_EXPORT __declspec(dllexport)
#else
#define GFX_IPC_BUFFER_TYPES_EXPORT __declspec(dllimport)
#endif  // defined(GFX_IPC_BUFFER_TYPES_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GFX_IPC_BUFFER_TYPES_IMPLEMENTATION)
#define GFX_IPC_BUFFER_TYPES_EXPORT __attribute__((visibility("default")))
#else
#define GFX_IPC_BUFFER_TYPES_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define GFX_IPC_BUFFER_TYPES_EXPORT
#endif

#endif  // UI_GFX_IPC_BUFFER_TYPES_GFX_IPC_EXPORT_H_
