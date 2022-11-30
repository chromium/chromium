// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EXTRA_AURA_EXTRA_EXPORT_H_
#define UI_AURA_EXTRA_AURA_EXTRA_EXPORT_H_

// Defines AURA_EXTRA_EXPORT so that functionality implemented by the aura-extra
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(AURA_EXTRA_IMPLEMENTATION)
#define AURA_EXTRA_EXPORT __declspec(dllexport)
#else
#define AURA_EXTRA_EXPORT __declspec(dllimport)
#endif  // defined(AURA_EXTRA_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(AURA_EXTRA_IMPLEMENTATION)
#define AURA_EXTRA_EXPORT __attribute__((visibility("default")))
#else
#define AURA_EXTRA_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define AURA_EXTRA_EXPORT
#endif

#endif  // UI_AURA_EXTRA_AURA_EXTRA_EXPORT_H_
