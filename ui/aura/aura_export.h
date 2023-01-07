// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_AURA_EXPORT_H_
#define UI_AURA_AURA_EXPORT_H_

// Defines AURA_EXPORT so that functionality implemented by the aura module
// can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(AURA_IMPLEMENTATION)
#define AURA_EXPORT __declspec(dllexport)
#else
#define AURA_EXPORT __declspec(dllimport)
#endif  // defined(AURA_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(AURA_IMPLEMENTATION)
#define AURA_EXPORT __attribute__((visibility("default")))
#else
#define AURA_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define AURA_EXPORT
#endif

#endif  // UI_AURA_AURA_EXPORT_H_
