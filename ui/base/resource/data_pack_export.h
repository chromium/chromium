// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_DATA_PACK_EXPORT_H_
#define UI_BASE_RESOURCE_DATA_PACK_EXPORT_H_

// Defines UI_DATA_PACK_EXPORT so that functionality implemented by the data
// pack loading module can be exported to consumers.

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(UI_DATA_PACK_IMPLEMENTATION)
#define UI_DATA_PACK_EXPORT __declspec(dllexport)
#else
#define UI_DATA_PACK_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(UI_DATA_PACK_IMPLEMENTATION)
#define UI_DATA_PACK_EXPORT __attribute__((visibility("default")))
#else
#define UI_DATA_PACK_EXPORT
#endif

#endif

#else  // !defined(COMPONENT_BUILD)

#define UI_DATA_PACK_EXPORT

#endif

#endif  // UI_BASE_RESOURCE_DATA_PACK_EXPORT_H_
