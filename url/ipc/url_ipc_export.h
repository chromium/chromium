// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_IPC_URL_IPC_EXPORT_H_
#define URL_IPC_URL_IPC_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(URL_IPC_IMPLEMENTATION)
#define URL_IPC_EXPORT __declspec(dllexport)
#else
#define URL_IPC_EXPORT __declspec(dllimport)
#endif  // defined(URL_IPC_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(URL_IPC_IMPLEMENTATION)
#define URL_IPC_EXPORT __attribute__((visibility("default")))
#else
#define URL_IPC_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define URL_IPC_EXPORT
#endif

#endif  // URL_IPC_URL_IPC_EXPORT_H_
